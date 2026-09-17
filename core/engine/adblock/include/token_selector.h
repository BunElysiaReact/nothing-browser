#pragma once
#include "tokenizer.h"
#include <unordered_map>
#include <vector>

namespace adblock {

// Pre-seeded bad tokens that are too common to be useful as bucket keys
// These are seeded with huge counts so real tokens always look rarer
static const std::vector<std::string> WORST_TOKENS = {
    "https", "http", "www", "com"
};

static const std::vector<std::string> BAD_TOKENS = {
    "uk", "net", "org", "io", "de", "fr", "es", "it",
    "nl", "se", "ru", "pl", "co", "js", "css", "img",
    "jpg", "html", "png", "cdn", "static", "images",
    "api", "wp", "ad", "ads", "content", "doubleclick",
    "analytics", "assets", "id", "min", "amazon",
    "google", "googlesyndication", "googleapis"
};

static const size_t WORST_TOKEN_USAGE = SIZE_MAX / 2;
static const size_t BAD_TOKEN_USAGE   = SIZE_MAX / 4;

class TokenSelector {
public:
    TokenSelector();

    // Record that a token was seen (increases its usage count)
    void register_token(ShortHash token);

    // Pick the rarest token from a list of candidates
    // Returns 0 if no good token found (falls back to catch-all)
    ShortHash select_least_used(const std::vector<ShortHash>& tokens);

private:
    std::unordered_map<ShortHash, size_t> usage_map;
};

} // namespace adblock 
