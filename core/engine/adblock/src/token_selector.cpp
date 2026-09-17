#include "../include/token_selector.h"
#include <limits>

namespace adblock {

TokenSelector::TokenSelector() {
    // Pre-seed worst tokens with near-MAX counts
    // so any real token always looks rarer than these
    for (const auto& t : WORST_TOKENS)
        usage_map[to_short_hash(sea_hash(t))] = WORST_TOKEN_USAGE;

    for (const auto& t : BAD_TOKENS)
        usage_map[to_short_hash(sea_hash(t))] = BAD_TOKEN_USAGE;
}

void TokenSelector::register_token(ShortHash token) {
    // Token 0 is reserved for catch-all — never track it
    if (token == 0) return;
    usage_map[token]++;
}

ShortHash TokenSelector::select_least_used(const std::vector<ShortHash>& tokens) {
    ShortHash best_token = 0;
    size_t best_count = std::numeric_limits<size_t>::max();

    for (ShortHash token : tokens) {
        // Skip catch-all bucket
        if (token == 0) continue;

        auto it = usage_map.find(token);

        // Never seen before = rarest possible = return immediately
        if (it == usage_map.end()) return token;

        if (it->second < best_count) {
            best_count = it->second;
            best_token = token;
        }
    }

    return best_token;
}

} // namespace adblock
