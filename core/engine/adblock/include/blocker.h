#pragma once
#include "network_filter.h"
#include "flat_multimap.h"
#include "token_selector.h"
#include "request.h"
#include <vector>
#include <optional>
#include <string>

namespace adblock {

struct BlockerResult {
    bool should_block  = false;
    bool is_important  = false;
    bool is_exception  = false;
    std::string matched_rule;  // the raw rule that matched, for debugging
};

class Blocker {
public:
    // Load filter rules from a string (one rule per line)
    void load(const std::string& filter_list);

    // Load a single rule
    void add_rule(const std::string& rule);

    // Must call after all rules are added
    void finalize();

    // Check a request against all loaded rules
    BlockerResult check(const Request& req) const;

private:
    // The 4 main lists
    FlatMultiMap<NetworkFilter> filters;       // normal block rules
    FlatMultiMap<NetworkFilter> exceptions;    // @@ exception rules
    FlatMultiMap<NetworkFilter> importants;    // $important rules
    FlatMultiMap<NetworkFilter> tagged;        // $tag= rules

    TokenSelector selector;
    bool finalized = false;

    // Insert a filter into the correct list
    void insert_filter(NetworkFilter& f);

    // Get the bucket key for a filter
    ShortHash get_bucket_key(const NetworkFilter& f) const;

    // Check a single filter list against a request
    std::optional<const NetworkFilter*> check_list(
        const FlatMultiMap<NetworkFilter>& list,
        const Request& req) const;

    // Check if a single filter matches a request
    bool filter_matches(const NetworkFilter& f, const Request& req) const;

    // Pattern matching helpers
    bool check_options(const NetworkFilter& f, const Request& req) const;
    bool check_domains(const NetworkFilter& f, const Request& req) const;
    bool check_pattern(const NetworkFilter& f, const Request& req) const;
    bool is_anchored_by_hostname(const std::string& filter_host,
                                  const std::string& host) const;
};

} // namespace adblock
