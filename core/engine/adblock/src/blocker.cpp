#include "../include/blocker.h"
#include <sstream>
#include <algorithm>

namespace adblock {

void Blocker::load(const std::string& filter_list) {
    std::istringstream stream(filter_list);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        add_rule(line);
    }
}

void Blocker::add_rule(const std::string& rule) {
    NetworkFilter f;
    if (NetworkFilter::parse(rule, f))
        insert_filter(f);
}

ShortHash Blocker::get_bucket_key(const NetworkFilter& f) const {
    auto tokens = tokenize(f.hostname + f.pattern);
    return const_cast<TokenSelector&>(selector).select_least_used(tokens);
}

void Blocker::insert_filter(NetworkFilter& f) {
    ShortHash key = get_bucket_key(f);
    if (f.mask & IS_IMPORTANT)
        importants.insert(key, f);
    else if (f.mask & IS_EXCEPTION)
        exceptions.insert(key, f);
    else
        filters.insert(key, f);
}

void Blocker::finalize() {
    filters.finalize();
    exceptions.finalize();
    importants.finalize();
    tagged.finalize();
    finalized = true;
}

bool Blocker::check_options(const NetworkFilter& f, const Request& req) const {
    // Check scheme
    if (req.is_https && !(f.mask & FROM_HTTPS)) return false;
    if (req.is_http  && !(f.mask & FROM_HTTP))  return false;

    // Check party
    if (req.is_third_party  && !(f.mask & THIRD_PARTY)) return false;
    if (!req.is_third_party && !(f.mask & FIRST_PARTY))  return false;

    // Check request type
    uint32_t type_bit = 0;
    switch (req.type) {
        case RequestType::Image:          type_bit = FROM_IMAGE;          break;
        case RequestType::Script:         type_bit = FROM_SCRIPT;         break;
        case RequestType::Stylesheet:     type_bit = FROM_STYLESHEET;     break;
        case RequestType::XmlHttpRequest: type_bit = FROM_XMLHTTPREQUEST; break;
        case RequestType::Font:           type_bit = FROM_FONT;           break;
        case RequestType::Media:          type_bit = FROM_MEDIA;          break;
        case RequestType::Websocket:      type_bit = FROM_WEBSOCKET;      break;
        case RequestType::Subdocument:    type_bit = FROM_SUBDOCUMENT;    break;
        case RequestType::Document:       type_bit = FROM_DOCUMENT;       break;
        default:                          type_bit = FROM_OTHER;          break;
    }
    if (type_bit && !(f.mask & type_bit)) return false;

    return true;
}

bool Blocker::check_domains(const NetworkFilter& f, const Request& req) const {
    if (f.opt_domains.empty() && f.opt_not_domains.empty()) return true;
    if (req.source_hashes.empty()) return f.opt_domains.empty();

    // Check excluded domains first
    for (Hash h : req.source_hashes) {
        for (Hash excl : f.opt_not_domains)
            if (h == excl) return false;
    }

    // Check included domains
    if (!f.opt_domains.empty()) {
        for (Hash h : req.source_hashes) {
            for (Hash incl : f.opt_domains)
                if (h == incl) return true;
        }
        return false;
    }

    return true;
}

bool Blocker::is_anchored_by_hostname(
    const std::string& filter_host,
    const std::string& host) const
{
    if (filter_host.empty()) return true;
    if (filter_host.size() > host.size()) return false;
    if (filter_host.size() == host.size()) return filter_host == host;

    // Find filter_host as substring of host
    size_t pos = host.find(filter_host);
    if (pos == std::string::npos) return false;

    // Prefix match — next char must be '.'
    if (pos == 0)
        return host[filter_host.size()] == '.';

    // Suffix match — prev char must be '.'
    if (pos == host.size() - filter_host.size())
        return host[pos - 1] == '.';

    // Infix match — both sides must be '.'
    return host[pos - 1] == '.' && host[filter_host.size() + pos] == '.';
}

bool Blocker::check_pattern(const NetworkFilter& f, const Request& req) const {
    const std::string& url = req.url_lower;

    // Hostname anchor
    if (f.mask & IS_HOSTNAME_ANCHOR) {
        if (!is_anchored_by_hostname(f.hostname, req.hostname))
            return false;
        if (f.pattern.empty() || f.pattern == "^") return true;
    }

    // No pattern — hostname match is enough
    if (f.pattern.empty()) return true;

    // Left + right anchor — full URL equality
    if ((f.mask & IS_LEFT_ANCHOR) && (f.mask & IS_RIGHT_ANCHOR))
        return url == f.pattern;

    // Left anchor — URL must start with pattern
    if (f.mask & IS_LEFT_ANCHOR)
        return url.substr(0, f.pattern.size()) == f.pattern;

    // Right anchor — URL must end with pattern
    if (f.mask & IS_RIGHT_ANCHOR)
        return url.size() >= f.pattern.size() &&
               url.substr(url.size() - f.pattern.size()) == f.pattern;

    // Plain contains
    return url.find(f.pattern) != std::string::npos;
}

bool Blocker::filter_matches(const NetworkFilter& f, const Request& req) const {
    return check_options(f, req) &&
           check_domains(f, req) &&
           check_pattern(f, req);
}

std::optional<const NetworkFilter*> Blocker::check_list(
    const FlatMultiMap<NetworkFilter>& list,
    const Request& req) const
{
    for (ShortHash token : req.tokens) {
        auto candidates = list.get(token);
        for (const NetworkFilter* f : candidates) {
            if (filter_matches(*f, req))
                return f;
        }
    }
    return std::nullopt;
}

BlockerResult Blocker::check(const Request& req) const {
    BlockerResult result;

    // 1. Check importants first — bypass exceptions
    auto important_match = check_list(importants, req);
    if (important_match) {
        result.should_block  = true;
        result.is_important  = true;
        result.matched_rule  = (*important_match)->raw_line;
        return result;
    }

    // 2. Check normal block filters
    auto block_match = check_list(filters, req);
    if (!block_match) return result;

    result.matched_rule = (*block_match)->raw_line;

    // 3. Check exceptions
    auto exception_match = check_list(exceptions, req);
    if (exception_match) {
        result.should_block = false;
        result.is_exception = true;
        result.matched_rule = (*exception_match)->raw_line;
        return result;
    }

    result.should_block = true;
    return result;
}

} // namespace adblock
