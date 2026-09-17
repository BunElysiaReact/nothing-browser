#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace adblock {

using Hash = uint64_t;
using ShortHash = uint32_t;

// Converts a 64-bit hash to a 32-bit short hash
ShortHash to_short_hash(Hash h);

// SeaHash — stable across platforms, same as Brave's implementation
Hash sea_hash(const std::string_view input);

// Check if a character is a valid token character (alphanumeric only)
bool is_token_char(char c);

// Tokenize a URL into a list of hashes
// Skips tokens that are:
// - length <= 1
// - adjacent to wildcards (*)
// - in the first/last position when anchored
std::vector<ShortHash> tokenize(const std::string_view url);

} // namespace adblock