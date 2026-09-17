#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace adblock {

// Bitmask flags for NetworkFilter
enum NetworkFilterMask : uint32_t {
    FROM_IMAGE          = 1 << 0,
    FROM_MEDIA          = 1 << 1,
    FROM_OBJECT         = 1 << 2,
    FROM_OTHER          = 1 << 3,
    FROM_PING           = 1 << 4,
    FROM_SCRIPT         = 1 << 5,
    FROM_STYLESHEET     = 1 << 6,
    FROM_SUBDOCUMENT    = 1 << 7,
    FROM_WEBSOCKET      = 1 << 8,
    FROM_XMLHTTPREQUEST = 1 << 9,
    FROM_FONT           = 1 << 10,
    FROM_HTTP           = 1 << 11,
    FROM_HTTPS          = 1 << 12,
    IS_IMPORTANT        = 1 << 13,
    MATCH_CASE          = 1 << 14,
    THIRD_PARTY         = 1 << 16,
    FIRST_PARTY         = 1 << 17,
    IS_REGEX            = 1 << 18,
    IS_LEFT_ANCHOR      = 1 << 19,
    IS_RIGHT_ANCHOR     = 1 << 20,
    IS_HOSTNAME_ANCHOR  = 1 << 21,
    IS_EXCEPTION        = 1 << 22,
    IS_COMPLETE_REGEX   = 1 << 24,
    IS_HOSTNAME_REGEX   = 1 << 28,
    FROM_DOCUMENT       = 1 << 29,
};

// Composite masks
static const uint32_t FROM_NETWORK_TYPES =
    FROM_IMAGE | FROM_MEDIA | FROM_OBJECT | FROM_OTHER |
    FROM_PING  | FROM_SCRIPT | FROM_STYLESHEET | FROM_SUBDOCUMENT |
    FROM_WEBSOCKET | FROM_XMLHTTPREQUEST | FROM_FONT;

static const uint32_t FROM_ALL_TYPES =
    FROM_NETWORK_TYPES | FROM_DOCUMENT;

static const uint32_t DEFAULT_MASK =
    FROM_NETWORK_TYPES | FROM_HTTP | FROM_HTTPS |
    THIRD_PARTY | FIRST_PARTY;

// The parsed network filter
struct NetworkFilter {
    uint32_t mask = DEFAULT_MASK;
    std::string hostname;           // extracted from || anchor
    std::string pattern;            // URL pattern to match
    std::vector<Hash> opt_domains;      // $domain= positive list
    std::vector<Hash> opt_not_domains;  // $domain=~x negative list
    std::optional<std::string> modifier; // $redirect=, $csp=, $removeparam=
    std::string raw_line;           // original rule for debugging

    // Convenience checks
    bool is_exception()       const { return mask & IS_EXCEPTION; }
    bool is_important()       const { return mask & IS_IMPORTANT; }
    bool is_hostname_anchor() const { return mask & IS_HOSTNAME_ANCHOR; }
    bool is_regex()           const { return mask & IS_REGEX; }
    bool is_third_party()     const { return mask & THIRD_PARTY; }
    bool is_first_party()     const { return mask & FIRST_PARTY; }

    // Parse a filter line into a NetworkFilter
    // Returns false if the line is invalid or a comment
    static bool parse(const std::string& line, NetworkFilter& out);
};

// Check if a pattern contains regex-like characters
bool check_is_regex(const std::string_view pattern);

} // namespace adblock
