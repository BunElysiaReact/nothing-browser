#include "../include/request.h"
#include <algorithm>
#include <cctype>

namespace adblock {

std::string Request::extract_hostname(const std::string& url) {
    // Find scheme end (://)
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return "";
    size_t host_start = scheme_end + 3;

    // Find end of hostname (/ or : or end of string)
    size_t host_end = url.find_first_of("/:?#", host_start);
    if (host_end == std::string::npos) host_end = url.size();

    return url.substr(host_start, host_end - host_start);
}

std::string Request::get_etld_plus1(const std::string& hostname) {
    // Simple eTLD+1 — find last two labels
    // e.g. "a.b.example.com" → "example.com"
    size_t last_dot = hostname.rfind('.');
    if (last_dot == std::string::npos) return hostname;
    size_t second_dot = hostname.rfind('.', last_dot - 1);
    if (second_dot == std::string::npos) return hostname;
    return hostname.substr(second_dot + 1);
}

std::vector<Hash> Request::compute_source_hashes(const std::string& hostname) {
    std::vector<Hash> hashes;
    if (hostname.empty()) return hashes;

    // Hash hostname + every parent suffix
    // "a.b.example.com" → hash each suffix
    size_t pos = 0;
    while (pos < hostname.size()) {
        std::string_view suffix = std::string_view(hostname).substr(pos);
        hashes.push_back(sea_hash(suffix));
        size_t dot = hostname.find('.', pos);
        if (dot == std::string::npos) break;
        pos = dot + 1;
    }

    return hashes;
}

Request Request::build(
    const std::string& url,
    const std::string& source_url,
    RequestType type)
{
    Request r;
    r.url  = url;
    r.type = type;

    // Lowercase the URL for matching
    r.url_lower = url;
    std::transform(r.url_lower.begin(), r.url_lower.end(),
                   r.url_lower.begin(), ::tolower);

    // Extract hostnames
    r.hostname        = extract_hostname(r.url_lower);
    r.source_hostname = extract_hostname(source_url);

    // Scheme detection
    r.is_https = (url.substr(0, 8) == "https://");
    r.is_http  = (url.substr(0, 7) == "http://");

    // Third party check — compare eTLD+1 of dest vs source
    std::string dest_etld   = get_etld_plus1(r.hostname);
    std::string source_etld = get_etld_plus1(r.source_hostname);
    r.is_third_party = (!dest_etld.empty() &&
                        !source_etld.empty() &&
                        dest_etld != source_etld);

    // Precompute tokens from lowercased URL
    r.tokens = tokenize(r.url_lower);

    // Precompute source hostname hashes
    r.source_hashes = compute_source_hashes(r.source_hostname);

    return r;
}

} // namespace adblock
