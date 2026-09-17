#pragma once
#include "tokenizer.h"
#include <vector>
#include <optional>
#include <algorithm>
#include <cstdint>

namespace adblock {

// A single entry in the multimap
template<typename T>
struct FlatEntry {
    ShortHash key;
    T value;
};

// FlatMultiMap — sorted vector of entries, binary search for lookup
// This is the core data structure for fast filter lookups
// Keys are ShortHash (u32 token hashes)
// Values are whatever filter type we store (NetworkFilter later)
template<typename T>
class FlatMultiMap {
public:
    // Insert a key-value pair
    void insert(ShortHash key, T value) {
        entries.push_back({key, value});
        sorted = false;
    }

    // Must call before any lookup — sorts entries by key
    void finalize() {
        std::sort(entries.begin(), entries.end(),
            [](const FlatEntry<T>& a, const FlatEntry<T>& b) {
                return a.key < b.key;
            });
        sorted = true;
    }

    // Get all values for a given key
    // Returns empty vector if key not found
    std::vector<const T*> get(ShortHash key) const {
        std::vector<const T*> results;

        // Binary search for first occurrence of key
        auto it = std::lower_bound(entries.begin(), entries.end(), key,
            [](const FlatEntry<T>& e, ShortHash k) {
                return e.key < k;
            });

        // Collect all entries with this key (they're contiguous)
        while (it != entries.end() && it->key == key) {
            results.push_back(&it->value);
            ++it;
        }

        return results;
    }

    size_t size() const { return entries.size(); }
    bool is_empty() const { return entries.empty(); }

private:
    std::vector<FlatEntry<T>> entries;
    bool sorted = false;
};

} // namespace adblock
