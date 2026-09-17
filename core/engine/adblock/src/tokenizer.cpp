#include "../include/tokenizer.h"
#include <cctype>

namespace adblock {

// SeaHash implementation
// Stable across platforms — critical for filter list compatibility
Hash sea_hash(const std::string_view input) {
    uint64_t a = 0x16f11fe89b0d677cULL;
    uint64_t b = 0xb480a793d8e6c86cULL;
    uint64_t c = 0x6fe2e5aaf078ebc9ULL;
    uint64_t d = 0x14f994a4c5259381ULL;

    auto diffuse = [](uint64_t x) -> uint64_t {
        x *= 0x6eed0e9da4d94a4fULL;
        uint64_t a = x >> 32;
        uint64_t b = x >> 60;
        x ^= a >> b;
        x *= 0x6eed0e9da4d94a4fULL;
        return x;
    };

    size_t i = 0;
    while (i + 8 <= input.size()) {
        uint64_t chunk = 0;
        for (int j = 0; j < 8; j++)
            chunk |= (uint64_t)(unsigned char)input[i + j] << (j * 8);
        a ^= chunk;
        a = diffuse(a);
        std::swap(a, b);
        std::swap(b, c);
        std::swap(c, d);
        i += 8;
    }

    // Handle remaining bytes
    uint64_t last = input.size();
    size_t remaining = input.size() - i;
    for (size_t j = 0; j < remaining; j++)
        last |= (uint64_t)(unsigned char)input[i + j] << (j * 8 + 8);

    a ^= last;
    a  = diffuse(a);
    b  = diffuse(b);
    c  = diffuse(c);
    d  = diffuse(d);

    return diffuse(a ^ b ^ c ^ d);
}

ShortHash to_short_hash(Hash h) {
    return static_cast<ShortHash>(h & 0xFFFFFFFF);
}

bool is_token_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c));
}

std::vector<ShortHash> tokenize(const std::string_view url) {
    std::vector<ShortHash> tokens;
    size_t i = 0;
    size_t n = url.size();

    while (i < n) {
        // Skip non-token characters
        if (!is_token_char(url[i])) {
            i++;
            continue;
        }

        // Find end of token
        size_t start = i;
        while (i < n && is_token_char(url[i])) i++;
        size_t len = i - start;

        // Skip tokens that are too short
        if (len <= 1) continue;

        // Skip tokens adjacent to wildcards
        bool prev_wildcard = (start > 0 && url[start - 1] == '*');
        bool next_wildcard = (i < n && url[i] == '*');
        if (prev_wildcard || next_wildcard) continue;

        // Hash and store the token
        std::string_view token = url.substr(start, len);
        tokens.push_back(to_short_hash(sea_hash(token)));
    }

    // Always append 0 — the catch-all bucket
    tokens.push_back(0);

    return tokens;
}

} // namespace adblock