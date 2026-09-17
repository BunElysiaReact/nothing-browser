This adblock was made from the core idea of brave browser rust add blocker here is the fincings 

# Brave adblock-rust → C++ Rewrite Guide

This is a comprehensive walkthrough of the engine's architecture and algorithms, organized by what you'll need to reimplement. I'll reference real code so you can go look at the source when you want exact details.

---

## 1. Filter List Parsing

### 1.1 The input format

The engine consumes **ABP / uBlock Origin–style text filter lists**, one rule per line. There are two flavors:

```
FilterFormat::Standard   →  ABP/uBO syntax
FilterFormat::Hosts      →  "127.0.0.1 evil.com" or bare "evil.com"
                            (synthesized into "||evil.com^")
```

Example rules:

**Network filters** (block / allow / modify requests):
```
||example.com^                          // block, hostname-anchored, separator-anchored
||example.com^$third-party              // only 3rd-party requests
||ads.example.com^$script,domain=a.com|b.com
@@||example.com^                        // exception (allowlist) — prefix @@
||example.com^$important                // overrides exceptions
||example.com^$redirect=noop.js         // redirect to a resource
||example.com^$csp=...                  // inject CSP header
||example.com^$removeparam=utm_source   // strip a query param
||example.com^$badfilter                // disable a matching rule
/regex-pattern/                         // full regex (only with feature flag)
|https://example.com/foo|               // exact left + right anchored URL
*ads*banner*                           // plain wildcard
```

**Cosmetic filters** (DOM / CSS / scriptlets):
```
##.ads                                  // generic CSS hide
example.com##.ads                       // hostname-scoped hide
example.com,~foo.com##.ads              // mixed hostname scope
example.com#@#.ads                      // exception (unhide)
example.com#?#div:has(.ad)              // procedural CSS (ABP extended)
example.com##+js(scriptlet-name, arg)   // uBO scriptlet injection
foo.*##.ads                             // entity match
```

### 1.2 The dispatch: comment vs network vs cosmetic

The dispatcher lives in `lists.rs::detect_filter_type`. The logic is:

1. If the line is empty, starts with `!`, starts with `[Adblock`, or is a `#` followed by whitespace → it's a comment, skip it.
2. If the line starts with `|` or `@@|` → it's a **network filter** (the pipe anchor is the giveaway).
3. Otherwise, look for a `#` character. If, within 4 bytes after that `#`, there is *another* `#` → it's a **cosmetic filter** (the marker can be `##`, `#@#`, `#?#`, `#$?#`, `#%#` — all need two `#`s within 4 chars).
4. If the line contains `$$` → unsupported (AdGuard HTML filtering).
5. Default → it's a **network filter** (bare substring pattern like `banner`).

The 4-byte lookahead window is important — it's what lets `#$?#` (AdGuard) be recognized as cosmetic even though it has weird characters between the sharps.

### 1.3 Network filter parsing — the two-stage pipeline

Network filter parsing is split across two files:

**Stage A — lexing** (`filters/abstract_network.rs::AbstractNetworkFilter::parse`)

This produces a structural skeleton. It scans the line right-to-left for the *last* `$` (reverse-search so commas inside `domain=` lists don't confuse it), and splits there:

```
[exception prefix] [left anchor] [pattern body] [right anchor] $ [options]
```

- Strip leading `@@` if present → set `exception = true`.
- Find the last `$` → everything after it is the options string.
- Strip leading `||` (double-pipe → hostname anchor) or `|` (single-pipe → left anchor).
- Check if the body ends with `|` (right anchor). *Important:* this check is done on the body slice *before* the `$options`, so `|https://x^$third-party` ends with `^` not `|`.

**Stage B — interpretation** (`filters/network.rs::NetworkFilter::parse`)

This takes the structural skeleton and turns it into a `NetworkFilter` struct. The default mask starts as:

```
THIRD_PARTY | FIRST_PARTY | FROM_HTTPS | FROM_HTTP
```

i.e. "by default, match both 1st/3rd party, both http/https". Then it walks the options vector and sets bits accordingly.

### 1.4 The options system

Options are comma-separated, parsed in `abstract_network.rs::parse_filter_options`. Each token is examined for:
- Leading `~` → negation
- Split on first `=` into `(name, value)`
- Dispatch by name against a match table

**Option storage** is split across three places:

| Option family | Where stored |
|---|---|
| Content-type bits (`image`, `script`, `stylesheet`, etc.) | Two bitmasks: `cpt_mask_positive` and `cpt_mask_negative` (later merged into `mask`) |
| Method bits (`method=get\|head\|post`) | `method_mask_positive` / `method_mask_negative` |
| `domain=` / `from=` | Two `Vec<Hash>` lists: `opt_domains` (positive) and `opt_not_domains` (negated, leading `~`) |
| `to=` | `opt_to_domains` / `opt_not_to_domains` (currently parsed but ignored at insert time) |
| `redirect=`, `redirect-rule=`, `csp=`, `removeparam=` | Stored in `modifier_option: Option<&str>` + a flag bit in `features_mask` |
| `badfilter`, `important`, `match-case`, `generichide` | Flag bits in `mask` or `features_mask` |
| `tag=` | Stored as `tag: Option<&str>` |

**Critical post-option processing:**

```rust
mask |= cpt_mask_positive;   // merge positive types
// If any type was negated, implicitly enable ALL network types
if (cpt_mask_negative & FROM_NETWORK_TYPES) != NONE {
    mask |= FROM_NETWORK_TYPES;
}
// If no positive type set, apply to all (except removeparam)
if (cpt_mask_positive & FROM_ALL_TYPES).is_empty() {
    if IS_REMOVEPARAM { mask |= FROM_DOCUMENT | FROM_SUBDOCUMENT | FROM_XMLHTTPREQUEST; }
    else              { mask |= FROM_NETWORK_TYPES; }
}
mask &= !cpt_mask_negative;  // strip negated types
```

### 1.5 Hostname extraction for `||`-anchored filters

When the rule starts with `||`, the parser needs to split the body into "hostname part" and "rest of URL". The split point is:

- If the body contains `*` or `^` (is "regex-like"): split at the first occurrence of `/`, `^`, or `*`.
- Otherwise: split at the first `/`.
- If neither exists, the whole body is the hostname.

If the splitter was `*` (e.g. `||*.foo.com`), set `IS_HOSTNAME_REGEX` — this relaxes the label-alignment rule at match time.

Hostname is punycode-decoded via `idna::domain_to_ascii` so IDN rules work uniformly.

### 1.6 Cosmetic filter parsing

The cosmetic parser (`filters/cosmetic.rs::CosmeticFilter::parse`) is conceptually simpler:

1. **Find two `#` characters** in the line. Everything before the first `#` is the hostname list. Everything between the two `#`s is the marker. Everything after is the selector.

2. **Interpret the marker:**
   - `##` → plain hide
   - `#@#` → exception (unhide) — cannot be generic
   - `#?#` → procedural/extended CSS (sets `translate_abp_syntax = true`)
   - `#%#`, `#$?#` → AdGuard syntax, unsupported

3. **Parse hostname list** (before the first `#`): comma-split, recognize `~` (negation) and `.*` (entity). Each location is hashed with `fast_hash` and sorted into one of four buckets: `hostnames`, `not_hostnames`, `entities`, `not_entities`.

4. **Parse selector** after the second `#`:
   - If it starts with `+js(` → scriptlet injection. Store args as the "selector", set `SCRIPT_INJECT` bit. Cannot be generic.
   - Otherwise look for action suffixes (`:style(...)`, `:remove()`, `:remove-attr(...)`, `:remove-class(...)`) or ABP `{ ... }` style injection.
   - Validate as CSS — fast path is a regex `^[#.]?[A-Za-z_][\w-]*$`; slow path uses the `selectors` crate.

5. **The `hidden_generic_rule` trick:** A rule like `~example.com##.ads` only *excludes* a host. But semantically it should also apply to *every other host*. So the builder calls `hidden_generic_rule()` which clones the rule with `not_hostnames`/`not_entities` cleared, and inserts *both*: the negated version (under `hash("example.com")` with kind=Unhide) and the generic version (in the generic bucket).

### 1.7 The optimizer

After parsing, before serialization, there's a fusion pass (`optimizer.rs::optimize`). The goal: merge multiple simple filters into one `FilterPart::AnyOf` to reduce per-request lookup overhead.

A filter is **optimizable** if it has:
- No `$domain=` option
- No `||` hostname anchor
- No `$redirect`
- No `$csp`

The fusion groups filters by `(mask_bits, is_complete_regex)` and fuses each group into a single `NetworkFilter` with `FilterPart::AnyOf([pattern1, pattern2, ...])`. At match time, the matcher tries each pattern in the `AnyOf` list — and for regex patterns, it compiles them all into a single `RegexSet` so they're tested in one pass.

The output is sorted by filter `id` for deterministic output.

---

## 2. The "Bloom Filter" — Actually a Hash Index + Sorted Multimap

**Important correction up front:** There is *no actual Bloom filter anywhere in this codebase*. I searched — `grep -i bloom` returns zero hits. The "fast URL lookup" is achieved by a two-level exact scheme:

1. **Tokenize the URL** → produce a set of token hashes
2. **Look up each token in a sorted-vector binary-search multimap** → get candidate rules
3. **Run the precise matcher** on each candidate

The "filtering" is done by tokenization (cheap, lots of candidates discarded) and the rarest-token heuristic (minimizes the bucket size). No probabilistic data structure is involved.

### 2.1 The two container families

There are two parallel families of containers, both flatbuffer-serializable. They are chosen based on lookup pattern:

**Family A — Hash-based (open addressing, via `HashIndex`)**

Used by **cosmetic filters** (keyed by `String` selectors, looked up per-element on every page).

- `HashSetView<String>` — class/id selector lookups
- `HashMapView<String, _>` — complex class/id rules

Design (`flatbuffers/containers/hash_index.rs`):
- **Open addressing** with **triangular probing**: slot, slot+1, slot+3, slot+6, slot+10, … (step grows by 1 each iteration)
- Table size must be a **power of two** (so masking replaces modulo)
- **Load factor 25–50%** — growth doubles capacity, rehashes everything
- **Empty slots marked by default-constructed key** (empty string for `String`, `0` for `u32`)
- Inspired by Chromium's `closed_hash_map.h`

**Family B — Sorted-vector / binary-search (via `SortedIndex`)**

Used by **network filters** (keyed by `u32` token hashes, queried once per request).

- `FlatMultiMapView<u32, NetworkFilter>` — the network filter hot path
- `FlatSetView<u32>`, `FlatMapView<u32, _>` — various small sets/maps

Design (`flatbuffers/containers/flat_multimap.rs`):
- Two parallel arrays: `sorted_u32_keys[]` and `value_offsets[]`
- Lookup is `std::lower_bound` for the key, then linear scan while key matches
- All values for a given key are contiguous (sorted at build time)
- This is essentially `std::multimap` but cache-friendly

### 2.2 The hash function

There are *two distinct hash functions* layered, and getting them right matters for your C++ port:

**(a) Token-string hash — SeaHash** (`utils.rs`)

```cpp
Hash fast_hash(string_view input);   // returns u64
ShortHash to_short_hash(Hash h);     // returns low 32 bits as u32
```

SeaHash is chosen because it is **stable across runs and platforms**. You absolutely cannot use `std::hash` if it's randomized (e.g. MSVC's `std::hash<string>` was historically randomized). For your C++ port: implement SeaHash faithfully if you want to read Brave's serialized `.dat` files; otherwise any stable 64-bit non-cryptographic hash (FNV-1a, xxHash, MurmurHash3) works.

**(b) Slot-location hash — FxHasher** (Rust's fast non-cryptographic hash)

Only used inside `HashIndex` to find the slot for a *key* (typically a `String` for cosmetic rules). For the network filter hot path this is irrelevant because tokens already arrive as `ShortHash = u32`, and the multimap is sorted (not hashed).

### 2.3 Tokenization — the heart of fast lookup

The tokenizer is `utils.rs::fast_tokenizer_no_regex`. The rules:

```cpp
// A "token" is a maximal run of alphanumeric chars
// (everything else — '.', '/', '?', '&', '=', ':', '*', '%' — is a separator)
bool is_allowed(char c) { return isalnum(c); }
```

Walking the pattern, emit a token hash whenever you hit a separator *if* the token is "good enough":

- **Length > 1** — single-char tokens like `a` are too common (skip them)
- **Not adjacent to `*`** — tokens next to wildcards are unreliable anchors (the wildcard already permits arbitrary text, so the token is meaningless as a discriminator)
- **First/last tokens skipped** when the rule is anchored — for `|https://...` the leading `https` would always be present and useless

The token buffer is a **stack-allocated `ArrayVec<Hash, 256>`** — no heap allocation per request. The buffer is reused across requests via `TokensBuffer`.

### 2.4 The `TokenSelector` — picking the rarest token

At build time, the engine needs to pick *which* token from a filter's body to use as the bucket key. The rule: **pick the rarest token**, because that minimizes the bucket size at query time.

`filters/token_selector.rs` maintains a usage map `HashMap<ShortHash, usize>`. Two key behaviors:

1. **Pre-seeded bad tokens.** A list of 40 known-bad tokens is pre-populated with huge usage counts:

   ```rust
   const WORST_TOKENS: [&str; 4] = ["https", "http", "www", "com"];
   const WORST_TOKEN_USAGE: usize = usize::MAX / 2;  // ~2^63
   const BAD_TOKENS: [&str; 36] = ["uk","net","org","io","de","fr","es","it",
       "nl","se","ru","pl","co","js","css","img","jpg","html","png","cdn",
       "static","images","api","wp","ad","ads","content","doubleclick",
       "analytics","assets","id","min","amazon","google","googlesyndication",
       "googleapis"];
   const BAD_TOKEN_USAGE: usize = usize::MAX / 4;  // ~2^62
   ```

   These are seeded with near-`MAX` counts so any *real* token, even one seen many times, will look rarer.

2. **First-never-seen-token wins.** `select_least_used_token` returns immediately on the first token it encounters that has *never been seen before* — that's optimal. Otherwise it picks the lowest-count token.

3. **Token `0` is reserved.** It's the catch-all bucket key (see below). It is skipped during selection.

The list of "bad tokens" was built empirically — the comment in the code says: *"The list was built by logging each token that triggered `.matches()` call for some network filter in the benchmark."*

### 2.5 The build vs. query lifecycle

**Build side** (per filter):

```
parse filter → NetworkFilter struct
  ↓
get_tokens(filter) → FilterTokens enum
  ↓
FilterTokens::Empty (no usable tokens)
  → bucket key = 0 (catch-all)
  → insert into filter_map under key 0

FilterTokens::Other (has URL/hostname tokens)
  → pick rarest token via TokenSelector
  → bucket key = to_short_hash(selected_token)
  → insert into filter_map under that key

FilterTokens::OptDomains (single positive $domain=X)
  → bucket key = to_short_hash(hash("X"))
  → insert into opt_domains_map under that key
```

`get_tokens` has a clever three-tier strategy:

1. **Best case:** A single positive `$domain=X`. Use the domain hash as the key — this is the most selective key available.
2. **Default:** Tokenize the URL pattern + hostname (with anchor-aware skip rules). Pick the rarest.
3. **Fallback:** If still no tokens (e.g. pure wildcard `*`), use `0` (catch-all). If only `$domain=` lists remain and they fit, use all of them as keys (multi-bucket).

**Query side** (per request):

```
incoming URL
  ↓
Request::new(url, source_url, type, method)
  ↓ compute
url_lower_cased, hostname, source_hostname_hashes,
request_tokens = tokenize(url_lower_cased) + [0]
                              ↑
                  always append 0 so catch-all bucket is consulted
  ↓
For each token in request_tokens:
  → binary-search the sorted keys vector
  → if found, iterate the value-run
  → for each candidate, run filter.matches(request)
```

The trailing `0` is critical — without it, the catch-all bucket (containing rules with no usable tokens, like pure `*` or all-`$domain=` rules) would never be consulted.

### 2.6 Why flatbuffers?

The whole hash-index/multimap machinery is built on **flatbuffers** for three reasons:

1. **Zero-copy deserialization.** The compiled `.dat` file is `mmap`'d at startup. No parsing step — the engine just verifies the buffer and starts querying.
2. **Compact storage.** Filter data is laid out contiguously — no per-node heap allocation.
3. **Cache-friendly linear vectors.** The keys vector is a contiguous `uint32_t[]`. The engine even does an unsafe `reinterpret_cast<const uint32_t*>(bytes)` to skip the flatbuffer offset-following overhead and get a raw slice — this is only sound because the engine forces 8-byte alignment on the entire blob.

For your C++ port: if you don't care about reading Brave's `.dat` files, you can skip flatbuffers entirely and use `std::vector<uint32_t>` + `std::vector<Filter>` — the algorithm is identical. If you want cross-compatibility, you'll need to mirror the flatbuffer schema exactly.

---

## 3. Network Request Matching — Step by Step

### 3.1 The `Request` struct

Built once per incoming request (`request.rs::Request::new`). The fields populated from the URL:

```cpp
struct Request {
    RequestType request_type;          // image, script, document, ...
    optional<Method> method;            // GET / HEAD / POST
    bool is_http, is_https, is_supported;
    bool is_third_party;               // source eTLD+1 != dest eTLD+1
    string url;                        // case-preserved (for $match-case)
    string url_lower_cased;            // for normal rules
    string hostname;                   // punycode-encoded
    optional<vector<Hash>> source_hostname_hashes;  // source host + all suffixes
    vector<Hash> request_tokens;       // URL tokens + trailing 0
    string original_url;               // unmodified, for $removeparam
};
```

**Two derived fields worth understanding:**

**(a) `source_hostname_hashes`** — for `$domain=` matching. The source hostname is split on `.`, and each suffix is hashed:

```
source = "www.example.co.uk"
hashes = [ hash("www.example.co.uk"),
           hash("example.co.uk"),
           hash("co.uk"),
           hash("uk") ]
```

This way, a rule with `$domain=example.co.uk` (whose token is `to_short_hash(hash("example.co.uk"))`) gets found by binary-searching the source-hostname hashes. The match works because the source's parent-domain hash matches the rule's bucket key.

**(b) `request_tokens`** — precomputed on the lowercased URL, with a trailing `0`:

```cpp
vector<Hash> calculate_tokens(string url_lower) {
    ArrayVec<Hash, 256> buf;
    tokenize_pooled(url_lower, buf);
    buf.push(0);                  // forces catch-all bucket (key 0) to be consulted
    return buf.into_vec();
}
```

### 3.2 The full pipeline — `Blocker::check_parameterised`

This is the master orchestrator (`blocker.rs::check_parameterised`). The order matters:

```
1. Unsupported scheme? → return empty (data:, file:, javascript:)
2. Check $important rules first → importants().check()
     ↓ matched?
       Y → set important=true, skip exceptions
       N → continue
3. Check normal blocking rules:
   a. tagged_filters_all().check() with active tags
   b. if no match → filters().check() with empty tags
4. Check exceptions (unless important matched):
   - if a block matched: always check exceptions
   - if no match: only check if caller asserts a block (matched_rule flag)
5. Check redirects — ALWAYS runs, independent of block decision:
   - redirects().check_all() — collect ALL matching redirect rules
   - gather exception-redirects, filter them out
   - pick highest-priority surviving redirect (priority from "name:priority" syntax)
   - resolve resource body via ResourceStorage
6. Check removeparam (unless important matched):
   - removeparam().check_all()
   - remove matching query params from URL, rebuild rewritten_url
7. Check CSP (separate entry point, called only for Document/Subdocument requests):
   - csp().check_all() — collect all matching CSP rules
   - apply exceptions, join directives
8. Assemble BlockerResult { filter, exception, important, redirect, rewritten_url }
```

**Final block decision:**

```cpp
bool should_block() {
    return important || (filter.has_value() && !exception.has_value());
}
```

### 3.3 The bucketed multimap lookup — `NetworkFilterList::check`

Each list (block, exception, important, redirect, etc.) wraps two parallel multimaps:

```
filter_map         : multimap<ShortHash, NetworkFilter>   // keyed by URL token
opt_domains_map    : multimap<ShortHash, NetworkFilter>   // keyed by source-hostname hash
```

The check is a two-pass lookup:

```cpp
optional<CheckResult> check(Request& req, HashSet<string>& active_tags, RegexManager& rm) {
    // Pass 1: $domain=-anchored rules, keyed by source-hostname hashes
    auto found = match_filters(get_opt_domains_map(),
                               req.source_hostname_hashes,
                               req, active_tags, rm);
    if (found) return found;

    // Pass 2: pattern/hostname/catch-all/fallback buckets. URL tokens only.
    return match_filters(get_filter_map(),
                         req.request_tokens,    // includes trailing 0
                         req, active_tags, rm);
}
```

`match_filters` is the inner loop:

```cpp
for (Hash token : tokens) {
    auto iter = filter_map.get(to_short_hash(token));   // binary search
    if (!iter) continue;
    for (auto& fb_filter : *iter) {
        FlatNetworkFilter filter(&fb_filter, ctx);
        if (filter.matches(req, rm)
            && (filter.tag().empty() || active_tags.contains(filter.tag()))
            && on_match(CheckResult{filter.mask, filter.modifier_option, ...}))
        {
            return true;  // early-stop for check(); check_all() never stops
        }
    }
}
```

### 3.4 Per-rule matching — `NetworkFilter::matches()`

For each candidate rule pulled from a bucket, four sequential gates run:

```cpp
bool matches(Request& req, RegexManager& rm) {
    if (!check_options(mask, req)) return false;             // Gate 1: cheap bitmask
    if (!check_included_domains_mapped(include_domains, req, ...)) return false;  // Gate 2: $domain=
    if (!check_excluded_domains_mapped(exclude_domains, req, ...)) return false;   // Gate 3: $domain=~x
    return check_pattern(mask, patterns, hostname, key, req, rm);                  // Gate 4: actual pattern
}
```

**Gate 1 — `check_options`** (cheap bitmask):

```cpp
bool check_options(NetworkFilterMask mask, Request& req) {
    if (!mask.check_cpt_allowed(req.request_type)) return false;
    if (!mask.first_party() && !req.is_third_party) return false;  // 1p disabled, 1p req
    if (!mask.third_party() && req.is_third_party)   return false;  // 3p disabled, 3p req
    if (req.is_https && !mask.for_https()) return false;
    if (req.is_http  && !mask.for_http())  return false;
    if (!check_method_allowed(mask, req.method)) return false;
    return true;
}
```

Quirk: for `Document` requests, an exception rule matches even *without* the `FROM_DOCUMENT` bit — `check_cpt_allowed` has a special case.

**Gates 2 & 3 — domain checks:**

The include/exclude domain arrays are stored as `u32` indices into a global "unique domains" table (memory optimization — there are typically only a few thousand unique domain hashes, so a `u32` index is smaller than a `u64` hash).

```cpp
bool check_included_domains(span<u32> included, Request& req, HashMap<Hash,u32>& mapping) {
    if (included.empty()) return true;
    if (!req.source_hostname_hashes) return false;  // need a source host
    // ALL source_hashes must miss for the rule to fail
    return !all_of(req.source_hostname_hashes, [&](Hash h) {
        auto idx = mapping.find(h);
        return !idx || !bin_lookup(included, *idx);
    });
    // i.e. need at least one source-hash to hit the include list
}
```

The exclude check is symmetric: any source-hash hitting the exclude list → fail.

**Gate 4 — `check_pattern` (dispatched by mask bits):**

This is the most complex gate. It dispatches on the mask bits `IS_HOSTNAME_ANCHOR`, `IS_REGEX`, `IS_LEFT_ANCHOR`, `IS_RIGHT_ANCHOR`, `IS_COMPLETE_REGEX`, `IS_HOSTNAME_REGEX`:

```
if IS_HOSTNAME_ANCHOR:
    if IS_REGEX:
        hostname_anchor_regex_filter
    elif IS_LEFT_ANCHOR && IS_RIGHT_ANCHOR:
        hostname_left_right_anchor_filter
    elif IS_RIGHT_ANCHOR:
        hostname_right_anchor_filter
    elif IS_LEFT_ANCHOR:
        hostname_left_anchor_filter
    else:
        hostname_anchor_filter
elif IS_REGEX || IS_COMPLETE_REGEX:
    regex_filter
elif IS_LEFT_ANCHOR && IS_RIGHT_ANCHOR:
    left_right_anchor_filter     // full equality: url == pattern
elif IS_LEFT_ANCHOR:
    left_anchor_filter            // url.starts_with(pattern)
elif IS_RIGHT_ANCHOR:
    right_anchor_filter           // url.ends_with(pattern)
else:
    plain_filter                  // url.contains(pattern)
```

### 3.5 The hostname anchor — `is_anchored_by_hostname`

This is the trickiest primitive. ABP's `||foo.com` matches `foo.com`, `a.foo.com`, AND `foo.com.baz` — but only when *label-aligned*. The algorithm (`network_matchers.rs::is_anchored_by_hostname`):

```cpp
bool is_anchored_by_hostname(string_view filter_host, string_view host,
                              bool wildcard_filter_host) {
    if (filter_host.empty()) return true;       // empty hostname → always anchored
    if (filter_host.length() > host.length()) return false;
    if (filter_host.length() == host.length()) return filter_host == host;

    // Find filter_host as a substring of host
    auto match_index = memmem(host, filter_host);
    if (!match_index) return false;

    if (match_index == 0) {
        // Prefix match: must align to a label boundary on the right
        // i.e. filter_host ends with '.' OR the next char in host is '.'
        return wildcard_filter_host
            || filter_host.ends_with('.')
            || host[filter_host.length()] == '.';
    } else if (match_index == host.length() - filter_host.length()) {
        // Suffix match: must align to a label boundary on the left
        // i.e. filter_host starts with '.' OR the preceding char is '.'
        return filter_host.starts_with('.')
            || host[match_index - 1] == '.';
    } else {
        // Infix match: must align on BOTH sides
        return (wildcard_filter_host || filter_host.ends_with('.')
                || host[filter_host.length()] == '.')
            && (filter_host.starts_with('.') || host[match_index - 1] == '.');
    }
}
```

The `wildcard_filter_host` flag (`IS_HOSTNAME_REGEX` mask bit) is set when the parser hit a `*` right after `||` (e.g. `||*.foo.com`) — in that case label-alignment is relaxed.

### 3.6 The `^` separator

`^` matches **any character that is NOT** a letter, digit, `_`, `-`, `.`, or `%` — OR end-of-string. From the regex compilation (`regex_manager.rs`):

```
^  →  (?:[^\w\d\._%-]|$)   // when at end of pattern
^X →  (?:[^\w\._%-])X       // when followed by more pattern
```

So `/`, `?`, `&`, `=`, `:`, control chars, and end-of-string are all separators.

### 3.7 Wildcard `*` handling

There's **no literal `*` expansion in plain-pattern code paths**. Instead, the parser strips leading/trailing `*`:

```cpp
// Strip trailing '*'
if (pattern.ends_with('*')) pattern = pattern.substr(0, pattern.length() - 1);

// Strip leading '*' if not hostname-anchored
if (!is_hostname_anchor && pattern.starts_with('*')) {
    pattern = pattern.substr(1);
    mask.is_left_anchor = false;
}
```

If `*` appears *inside* the pattern (not at the boundaries), `check_is_regex(pattern)` returns true, and the matcher routes through the regex path. At regex compile time, `*` → `.*`.

### 3.8 Regex filters — `RegexManager`

When a pattern contains `*` or `^` (or is `/regex/`), the matcher delegates to `RegexManager`. Three mask bits:

- `IS_REGEX` — pattern contains `*` or `^`
- `IS_COMPLETE_REGEX` — pattern is `/regex/`
- `IS_HOSTNAME_REGEX` — hostname part contains `*` (e.g. `||*.foo.com`)

**Compilation** (`regex_manager.rs::make_regexp`):

```cpp
string compile_regex(string filter, bool left_anchor, bool right_anchor) {
    // 1. Escape regex special chars: | . $ + ? { } ( ) [ ] \
    filter = escape_specials(filter);
    // 2. Wildcard: * → .*
    filter = replace_all(filter, "*", ".*");
    // 3. Separator: ^ → (?:[^\w\d\._%-])  (or with |$ at end of pattern)
    filter = replace_separator(filter);
    // 4. Anchors
    if (left_anchor)  filter = "^" + filter;
    if (right_anchor) filter = filter + "$";
    return filter;
}
```

The engine uses Rust's `regex` crate (RE2-style, no backtracking, linear time). For C++, **Google's RE2** is the equivalent — same algorithm, same guarantees. Set `unicode(false)` so byte-level matching is used.

**Caching strategy:**

- Compiled regexes are cached by filter key (the flatbuffer offset, which is stable across requests).
- `CompiledRegex` is an enum: `Compiled(BytesRegex)` for single patterns, `CompiledSet(BytesRegexSet)` for `AnyOf` patterns (so all patterns in a fused filter are tested in one pass).
- LRU-style eviction: every 30s, regexes unused for 180s have their compiled form dropped (but the cache entry stays, so the filter identity is preserved — the regex will be recompiled on next access).

### 3.9 Exception handling

Exception rules (`@@||...`) are routed at parse time to a **separate list** (`NetworkFilterListId::Exceptions`). They never live in the `Filters` list. So block-check and exception-check are **two distinct list queries**, not a single merged scan.

The exception flag is a bit on the rule's mask: `IS_EXCEPTION = 1 << 22`. Set during parse when the rule starts with `@@`.

The order in `Blocker::check_parameterised`:
1. Check `importants` first (exceptions don't apply).
2. If no important match: check `tagged_filters_all` then `filters`.
3. Determine exception:
   - If a blocking filter matched (and is not important): always check `exceptions`.
   - If nothing matched: only check exceptions if caller asserts a block.

### 3.10 `$important` modifier

`$important` rules:
1. Are stored in their own list (`Importants`).
2. Are checked **first**, before any other list.
3. **Bypass exceptions entirely** — if important matched, the exception check is skipped.
4. Disable `$removeparam` (no URL rewriting).

The semantics (per the source comment): when one blocker instance returns `important=true`, the caller should stop forwarding the request to other instances. This is how Brave coordinates multiple blocker instances.

### 3.11 The eight-list split

At build time, every rule is categorized into one of eight lists:

```cpp
enum NetworkFilterListId {
    Csp = 0,
    Exceptions = 1,
    Importants = 2,
    Redirects = 3,
    RemoveParam = 4,
    Filters = 5,             // the "main" blocklist
    GenericHide = 6,         // $generichide exception rules
    TaggedFiltersAll = 7,    // rules with $tag=
    Size = 8,
};
```

This separation means the matcher only walks the relevant lists per query. A normal block check touches at most 4 lists (importants, tagged_filters_all, filters, exceptions); a CSP check only touches `Csp`; a redirect resolution touches `Redirects` + `Exceptions`.

**`$redirect` quirk:** `redirect` rules go into `Redirects` AND, if the option was `$redirect` (not `$redirect-rule`), also into `Filters` (to actually block). The `ALSO_BLOCK_REDIRECT` features_mask bit tracks this. `$redirect-rule` only goes into `Redirects`.

---

## 4. Cosmetic Filtering

### 4.1 The `CosmeticFilter` struct

```cpp
struct CosmeticFilter {
    optional<vector<Hash>> entities;          // e.g. "google.*" matches "google.com"
    optional<vector<Hash>> hostnames;         // positive hostname constraints
    CosmeticFilterMask mask;                 // u8: UNHIDE | SCRIPT_INJECT
    optional<vector<Hash>> not_entities;      // ~entity.* exceptions
    optional<vector<Hash>> not_hostnames;     // ~hostname exceptions
    optional<string> raw_line;                // debug only
    vector<CosmeticFilterOperator> selector;  // plain CSS or chain w/ procedural ops
    optional<CosmeticFilterAction> action;    // :remove, :style, etc.
    PermissionMask permission;                // scriptlet permission gating
};
```

Key point: **all hostname constraints are pre-hashed** (`Hash = u64`). Strings are never stored for hostnames — the engine only ever compares hashes at runtime. This is critical for memory and perf.

The `selector` field is a `Vec<CosmeticFilterOperator>` (not a plain string). For a plain CSS rule it's a single-element `[CssSelector(".ads")]`. For procedural rules (`#?#script:has-text(foo)`), it's a chain like `[CssSelector("script"), HasText("foo")]`.

```cpp
enum CosmeticFilterOperator {
    CssSelector,
    HasText,         // :has-text(...)
    MatchesAttr,     // :matches-attr(...)
    MatchesCss,      // :matches-css(...)
    MatchesCssBefore,
    MatchesCssAfter,
    MatchesPath,
    MinTextLength,
    Upward,
    Xpath,
};

enum CosmeticFilterAction {
    Remove,              // :remove()
    Style(string),       // :style(margin: 0)
    RemoveAttr(string),  // :remove-attr(onclick)
    RemoveClass(string), // :remove-class(advert)
};
```

### 4.2 Generic vs specific

A filter is **"generic"** iff it has no hostname constraint (no `hostnames`, `entities`, `not_hostnames`, or `not_entities`):

```cpp
bool has_hostname_constraint() {
    return hostnames || entities || not_entities || not_hostnames;
}
```

The builder keeps generic and hostname-specific rules in **completely separate storage** because the query patterns differ (generic → checked against every page's classes/ids; specific → only against pages with a matching hostname hash).

### 4.3 The `CosmeticFilterCacheBuilder` — storage layout

```
struct CosmeticFilterCacheBuilder {
    // --- GENERIC buckets (no hostname) ---
    HashSet<string> simple_class_rules;              // ".ad" (bare class, no descendant)
    HashSet<string> simple_id_rules;                 // "#ad" (bare id)
    HashSet<string> misc_generic_selectors;           // "div[class^='Ad']" etc.
    HashMap<string, StringVector> complex_class_rules; // ".ad > div" keyed by "ad"
    HashMap<string, StringVector> complex_id_rules;    // "#ad > div" keyed by "ad"

    // --- HOSTNAME-SPECIFIC buckets (keyed by hostname/entity hash) ---
    FlatMultiMapBuilder<Hash, string> hostname_hide;            // hide rules (high volume)
    FlatMultiMapBuilder<Hash, string> hostname_inject_script;  // +js scriptlets
    HashMap<Hash, HostnameRule> specific_rules;                 // unhide, uninject, procedural
};

struct HostnameRule {
    vector<string> unhide;                       // #@# exceptions
    vector<string> uninject_script;              // scriptlet exceptions
    vector<string> procedural_action;            // procedural filters (JSON)
    vector<string> procedural_action_exception;   // exceptions to procedural filters
};
```

**Why this split:**

- `simple_class_rules`/`simple_id_rules` exist because for a bare `.ad` rule, the renderer only needs to know "is `ad` a class on the page?" — no full selector string needs to be stored. `key_from_selector` extracts the leading class/id token.
- `complex_class_rules`/`complex_id_rules` store rules like `.ad > div` keyed by the leading class — so the renderer can look up "any complex rule starting with `.ad`" when it sees class `ad` on the page.
- `misc_generic_selectors` catches everything else (e.g. `div[class^="Ad"]`) — these are applied to every page.
- `hostname_hide` and `hostname_inject_script` are top-level multimaps (not in `specific_rules`) because they're the highest-volume cases — separating them allows better deduplication.
- `specific_rules` (the `HostnameRule` struct) holds the lower-volume cases: unhide, uninject, procedural_action, procedural_action_exception.

### 4.4 The negation-as-separate-bucket trick

This is the clever part. When a rule like `~example.com##.ads` is parsed:

1. The builder takes the *negated* `SpecificFilterType::Unhide(".ads")` and inserts it under `hash("example.com")`.

```cpp
void store_hostname_rule(CosmeticFilter& rule, Builder& b) {
    SpecificFilterType kind = classify(rule);  // Hide, InjectScript, ProceduralOrAction

    // Positive hostnames/entities → regular kind
    auto positive_tokens = chain(rule.hostnames, rule.entities);
    store_hostname_filter(positive_tokens, kind, b);

    // Negated hostnames/entities → NEGATED kind (Unhide, UninjectScript, etc.)
    auto negative_tokens = chain(rule.not_hostnames, rule.not_entities);
    store_hostname_filter(negative_tokens, kind.negated(), b);
}
```

So when the engine queries `example.com`, the unhide rules come back from the **same hash lookup** as the positive hide rules. No separate scan of `not_hostnames` is needed — the decision was made at build time.

### 4.5 The query path — `hostname_cosmetic_resources`

Public API: `Engine::url_cosmetic_resources(url)`. It:

1. Builds a `Request` from the URL.
2. Checks `generichide` — is there a `$generichide` exception matching this URL? If so, generic rules are skipped entirely.
3. Calls `CosmeticFilterCache::hostname_cosmetic_resources(resources, hostname, generichide)`.

**Step 1 — Compute candidate hostname/entity hashes:**

For URL `https://a.b.example.com` with domain `example.com`:
- `request_hostnames` = hashes of `[example.com, b.example.com, a.b.example.com]` (suffixes from the domain)
- `request_entities` = hashes of `[example.com, b.example, a.b.example]` (entity labels — last label dropped, treating each as `entity.*`)

So a rule `example.*##.ads` matches via the entity hash.

**Step 2 — First pass: gather positives:**

```cpp
for (Hash hash : chain(entities, hostnames)) {
    // Top-level hide selectors
    if (auto hide_iter = hostname_hide.get(hash)) {
        for (string sel : *hide_iter) {
            if (!exceptions.contains(sel))
                specific_hide.insert(sel);
        }
    }
    // Top-level script injections (decode permission from last 2 chars)
    if (auto script_iter = hostname_inject_script.get(hash)) {
        for (string encoded : *script_iter) {
            auto [perm, script] = decode_script_with_permission(encoded);
            script_injections[script] |= perm;  // merge permissions
        }
    }
    // Procedural actions (JSON strings)
    if (auto rules = hostname_rules.get(hash)) {
        for (string action : rules->procedural_action)
            procedural_actions.insert(action);
    }
}
```

**Step 3 — Second pass: apply exceptions:**

```cpp
for (Hash hash : chain(entities, hostnames)) {
    if (auto rules = hostname_rules.get(hash)) {
        // Unhide selectors: remove from hide set AND add to exceptions set
        for (string sel : rules->unhide) {
            specific_hide.erase(sel);
            exceptions.insert(sel);
        }
        // Procedural exceptions
        for (string action : rules->procedural_action_exception)
            procedural_actions.erase(action);
        // Script uninjects (empty string = wildcard: clear all)
        for (string script : rules->uninject_script) {
            if (script.empty()) {
                except_all_scripts = true;
                script_injections.clear();
            }
            if (!except_all_scripts) script_injections.erase(script);
        }
    }
}
```

The `exceptions` set is the **explicit contract** with the caller: it's used by `hidden_class_id_selectors` (below) to suppress generic rules that have been overridden by hostname exceptions.

**Step 4 — Merge with generic selectors (unless `generichide`):**

```cpp
if (generichide) {
    hide_selectors = specific_hide;    // skip generic entirely
} else {
    hide_selectors = specific_hide;
    for (string sel : misc_generic_selectors) {
        if (!exceptions.contains(sel))
            hide_selectors.insert(sel);
    }
}
```

**Step 5 — Resolve scriptlets and assemble:**

```cpp
UrlSpecificResources result {
    .hide_selectors      = hide_selectors,
    .procedural_actions  = procedural_actions,
    .exceptions          = exceptions,
    .injected_script     = resources.get_scriptlet_resources(script_injections),
    .generichide         = generichide,
};
```

### 4.6 The lazy/dynamic companion — `hidden_class_id_selectors`

The above gives you the *static* rules for a page. But pages have dynamic content — MutationObserver fires when new classes/ids appear. For that, the engine exposes:

```cpp
vector<string> hidden_class_id_selectors(
    iterable<string> classes,
    iterable<string> ids,
    const HashSet<string>& exceptions);
```

For each class on the page:
- If `simple_class_rules.contains(class)` and `exceptions` doesn't contain `.class` → return `.class`.
- If `complex_class_rules.get(class)` exists, return each selector in the list (minus exceptions).

Same for ids. This is O(1) per class/id via HashSet lookup, and is the contract for dynamic cosmetic filtering.

### 4.7 CSS injection — who produces the CSS?

**Important:** The engine itself does *not* produce a combined CSS string like `.ads,.banner{display:none!important}`. It returns a `HashSet<string>` of selectors, and the caller (the browser content script) is responsible for formatting/injecting.

The browser would produce CSS by joining selectors with `,` and applying `display: none !important`:

```css
.ads,.banner,.promo{display:none!important}
```

For `:style(...)` rules, each becomes its own rule: `.ad{margin:0!important}`.

### 4.8 Procedural / scriptlet filters

Three distinct categories:

**a) Scriptlet injections — `+js(...)`**

Stored in `hostname_inject_script` multimap, with permission bits encoded into the last 2 chars of the script string (avoids a separate permission map in the flatbuffer). At query time, `ResourceStorage::get_scriptlet_resources` looks up each scriptlet by name + permission, wraps each in `try { ... } catch (e) {}`, and concatenates with dependency scripts.

**b) Procedural CSS filters — `#?#`**

Use pseudo-classes like `:has-text()`, `:matches-css()`, `:xpath()`, `:upward()`. Stored as JSON-encoded `ProceduralOrActionFilter` objects in `hostname_rules.procedural_action`. The browser's content script must deserialize these and execute the procedural logic against the live DOM.

Restriction: a procedural filter may contain **only one** selector (no comma-separated selector lists).

**c) Action filters — `:remove()`, `:style(...)`, etc.**

Same storage as (b) — JSON-encoded, executed by the browser content script.

For any procedural/action filter that happens to be reducible to a single CSS selector + style, `ProceduralOrActionFilter::as_css()` returns `(selector, style)` so the browser can treat it as plain CSS.

---

## 5. Core Data Structures

### 5.1 The `Engine` struct

```cpp
struct Engine {
    Blocker blocker;
    CosmeticFilterCache cosmetic_cache;
    ResourceStorage resources;
    shared_ptr<FilterDataContext> filter_data_context;
};
```

Just four fields. The actual list buckets live in the flatbuffer, accessed through `filter_data_context`.

### 5.2 The `Blocker` struct

```cpp
struct Blocker {
    HashSet<string> tags_enabled;
    Mutex<RegexManager> regex_manager;   // or RefCell in single-thread mode
    shared_ptr<FilterDataContext> filter_data_context;
};
```

The blocker constructs transient `NetworkFilterList` views on demand:

```cpp
NetworkFilterList get_list(NetworkFilterListId id) {
    return NetworkFilterList {
        .list = filter_data_context->memory.root().network_rules().get(id),
        .filter_data_context = filter_data_context.get(),
    };
}
```

And exposes per-bucket getters: `csp()`, `exceptions()`, `importants()`, `redirects()`, `removeparam()`, `filters()`, `generic_hide()`, `tagged_filters_all()`.

### 5.3 The `NetworkFilterMask` — bit layout

This is a `u32` bitflags. The exact bit positions matter if you want to read Brave's serialized flatbuffers:

| Bit | Name | Meaning |
|---|---|---|
| 0 | FROM_IMAGE | apply to image requests |
| 1 | FROM_MEDIA | media |
| 2 | FROM_OBJECT | object/embed |
| 3 | FROM_OTHER | "other" |
| 4 | FROM_PING | beacon/ping |
| 5 | FROM_SCRIPT | script |
| 6 | FROM_STYLESHEET | stylesheet |
| 7 | FROM_SUBDOCUMENT | subdocument/iframe |
| 8 | FROM_WEBSOCKET | ws/wss |
| 9 | FROM_XMLHTTPREQUEST | xhr |
| 10 | FROM_FONT | font |
| 11 | FROM_HTTP | URL is http:// |
| 12 | FROM_HTTPS | URL is https:// |
| 13 | IS_IMPORTANT | $important — beats exceptions |
| 14 | MATCH_CASE | case-sensitive (only with /regex/) |
| 15 | *(unused)* | |
| 16 | THIRD_PARTY | apply to 3rd-party |
| 17 | FIRST_PARTY | apply to 1st-party |
| 18 | IS_REGEX | pattern contains `*` or `^` |
| 19 | IS_LEFT_ANCHOR | leading `\|` |
| 20 | IS_RIGHT_ANCHOR | trailing `\|` or `^` end-anchor |
| 21 | IS_HOSTNAME_ANCHOR | leading `\|\|` (domain anchor) |
| 22 | IS_EXCEPTION | `@@` exception rule |
| 23 | *(unused)* | |
| 24 | IS_COMPLETE_REGEX | `/regex/` form |
| 25 | UNMATCHED | mask for Csp request-type |
| 26 | FROM_GET | $method=get |
| 27 | FROM_HEAD | $method=head |
| 28 | IS_HOSTNAME_REGEX | hostname part contains `*` |
| 29 | FROM_DOCUMENT | top-level document |
| 30 | FROM_POST | $method=post |
| 31 | *(unused)* | |

Composite masks:
- `FROM_NETWORK_TYPES` = bits 0-10 (everything except document)
- `FROM_ALL_TYPES` = `FROM_NETWORK_TYPES | FROM_DOCUMENT`
- `DEFAULT_OPTIONS` = `FROM_NETWORK_TYPES | FROM_HTTP | FROM_HTTPS | THIRD_PARTY | FIRST_PARTY`

### 5.4 The `NetworkFilterFeaturesMask` (separate, NOT serialized)

```cpp
enum NetworkFilterFeaturesMask : uint32_t {
    BAD_FILTER         = 1 << 0,   // $badfilter — cancels matching rule
    IS_REMOVEPARAM     = 1 << 1,   // $removeparam
    GENERIC_HIDE       = 1 << 2,   // $generichide (exception only)
    IS_CSP             = 1 << 3,   // $csp
    IS_REDIRECT        = 1 << 4,   // $redirect or $redirect-rule
    ALSO_BLOCK_REDIRECT = 1 << 5,  // $redirect (also creates a block rule)
};
```

These are *build-time only*. They're not persisted to the flatbuffer because they're reflected in which list (bucket) the filter is stored in.

### 5.5 The `FilterPart` enum

```cpp
enum FilterPart {
    Empty,                 // no URL pattern (e.g. ||example.com^)
    Simple(string),        // exactly one pattern (~95% of filters)
    AnyOf(vector<string>), // multiple alternative patterns (from optimizer)
};
```

`Empty` is used for:
- Hostname-only rules (`||example.com^`)
- `$csp=...` rules with no pattern
- After optimization when a group includes a pattern that matches "everything"

`AnyOf` is created only by the optimizer — it fuses N rules with the same mask into one filter, and the matcher uses `RegexSet` to test all patterns at once.

### 5.6 The flatbuffer variant — `FbNetworkFilter`

The flatbuffer schema (`flatbuffers/fb_network_filter.fbs`):

```
table NetworkFilter {
  mask: uint32 = 540221439;            // NetworkFilterMask (default = common case)
  opt_domains: [uint32];               // indices into Engine.unique_domains_hashes
  opt_not_domains: [uint32];
  opt_to_domains: [uint32];
  opt_not_to_domains: [uint32];
  single_pattern: string;              // pattern (FilterPart::Simple)
  multi_patterns: [string];            // patterns (FilterPart::AnyOf)
  modifier_option: string;
  hostname: string;
  tag: string;
  raw_line: string;                    // debug-only
  source_index: uint32 = 4294967295;   // u32::MAX = "no info"
  line_number: uint32 = 4294967295;
}
```

Key differences from the in-memory `NetworkFilter`:
- **No `features_mask`** — those are build-time only.
- **`opt_domains` are `u32` indices**, not `u64` hashes. They index into the root-level `unique_domains_hashes: [uint64]` array. Memory optimization — there are typically few thousand unique domain hashes, so a `u32` index is smaller and allows sharing across filters.
- **`single_pattern` vs `multi_patterns`** — chosen at serialize time by `filter_count <= 1`. The runtime reader restores `Empty | Single(&str) | Multi(Vector<&str>)`.
- **No `id` field** — instead, the flatbuffer offset (`filter._tab.loc() as u64`) is used as the regex-cache key.

### 5.7 The `FilterDataContext`

```cpp
struct FilterDataContext {
    VerifiedFlatbufferMemory memory;       // mmap'd or owned bytes
    HashMap<Hash, uint32_t> unique_domains_hashes_map;  // rebuilt at deserialization
    bool debug;
};
```

Shared by `Blocker` and `CosmeticFilterCache` via `shared_ptr`. The `unique_domains_hashes_map` is rebuilt at deserialization time so the matcher can map a runtime hash → u32 index into the flatbuffer's domain arrays.

### 5.8 The `RegexManager`

```cpp
struct RegexManager {
    HashMap<uint64_t, RegexEntry> map;       // key = flatbuffer offset of filter
    size_t compiled_regex_count;
    Instant now, last_cleanup;
    RegexManagerDiscardPolicy discard_policy;
};

struct RegexEntry {
    optional<CompiledRegex> regex;          // None if evicted
    Instant last_used;
    size_t usage_count;
};

enum CompiledRegex {
    Compiled(BytesRegex),                   // single pattern
    CompiledSet(BytesRegexSet),             // AnyOf patterns
    MatchAll,                               // empty pattern → always true
    RegexParsingError,
};
```

- Lazily compiles wildcard patterns to regex.
- Caches by filter key (stable across requests).
- LRU-style eviction: every 30s, regexes unused for 180s have their compiled form dropped (the entry stays, regex is recompiled on next access).
- Uses Rust's `regex` crate (RE2-style). For C++, use **Google's RE2** — same algorithm, same linear-time guarantees.

### 5.9 The `.dat` file format

```
+-------------------+
| magic (4 bytes)   |   = [0xd1, 0xd9, 0x3a, 0xaf]
|                   |   = first 4 bytes of sha512("brave/adblock-rust")
+-------------------+
| version (1 byte)  |   = 7 (ADBLOCK_RUST_DAT_VERSION)
+-------------------+
| seahash (8 bytes) |   = seahash of the data
+-------------------+
| data (rest)       |   = the flatbuffer bytes
+-------------------+
```

The version byte is bumped on any breaking change to the flatbuffer schema or hash function. A stale `.dat` file is rejected quickly via version check, causing the browser to rebuild from list text rather than risk misinterpreting old data.

---

## 6. The Matching Pipeline — End to End

Here's the complete pipeline from incoming URL to block/allow decision:

### Phase 1: Build the Engine (one-time)

```
1. Parse each list line:
   - detect_filter_type(line) → comment | network | cosmetic
   - NetworkFilter::parse(line) or CosmeticFilter::parse(line)
   - on parse error: log and skip

2. For each network filter, categorize into one of 8 lists:
   - is_badfilter? → record ID in bad_filter_ids set, don't serialize
   - has_to_option? → drop (currently unsupported)
   - is_redirect? → also add to Redirects list
   - pick list_id by category:
     is_csp           → Csp
     is_removeparam   → RemoveParam
     is_generic_hide  → GenericHide
     is_exception     → Exceptions
     is_important     → Importants
     has tag          → TaggedFiltersAll
     else             → Filters

3. For each filter in each list, compute tokens via get_tokens():
   - FilterTokens::Empty → bucket key = 0 (catch-all)
   - FilterTokens::Other → pick rarest token via TokenSelector → bucket key = short_hash(token)
   - FilterTokens::OptDomains → bucket key = short_hash(domain_hash), insert into opt_domains_map

4. (Optional) Run optimizer on Filters list:
   - Group by (mask_bits, is_complete_regex)
   - Fuse each group into FilterPart::AnyOf

5. Serialize:
   - For each list: build FlatMultiMapBuilder, sort by key, write parallel vectors
   - Build unique_domains_hashes array (dedupe all domain hashes)
   - Write fb::Engine root with network_rules + cosmetic_filters + unique_domains_hashes
   - Wrap in .dat envelope (magic + version + seahash + flatbuffer)

6. (At load time) mmap the .dat, verify, wrap in FilterDataContext
7. (At load time) Rebuild unique_domains_hashes_map (hash → u32 index)
8. Create Blocker and CosmeticFilterCache views over the context
```

### Phase 2: Handle an incoming request

```
1. Caller invokes: engine.check_network_request(request)
   where request = Request::new(url, source_url, type, method)

2. Request construction:
   - Parse URL (scheme, host, path)
   - Punycode-encode hostname
   - Compute is_third_party (source eTLD+1 != dest eTLD+1)
   - Compute source_hostname_hashes (source host + all suffixes, each hashed)
   - Compute request_tokens = tokenize(url_lower_cased) + [0]

3. Blocker::check_parameterised(request):
   3a. Unsupported scheme? → return empty
   3b. Check $important list first:
       For each token in request.request_tokens + source_hostname_hashes:
         binary-search the list's filter_map / opt_domains_map
         for each candidate: run filter.matches(request)
           - check_options (cheap bitmask)
           - check_included_domains_mapped ($domain=)
           - check_excluded_domains_mapped ($domain=~x)
           - check_pattern (dispatch by mask bits)
         if matched and tag active → return CheckResult
       If important matched → set important=true, skip exceptions

   3c. If no important match: check tagged_filters_all, then filters
       (same lookup pipeline as 3b)

   3d. Check exceptions (if block matched or forced):
       - If matched filter was $important → skip (important bypasses exceptions)
       - Otherwise: same lookup pipeline on Exceptions list
       - If exception matched → block decision is overridden

   3e. Check redirects (always, independent of block):
       - redirects().check_all() — collect ALL matching redirect rules
       - gather exception-redirects, filter them out
       - pick highest-priority surviving redirect (priority from "name:priority")
       - resolve resource body via ResourceStorage → data: URL

   3f. Check removeparam (unless important matched):
       - removeparam().check_all()
       - remove matching query params, rebuild rewritten_url

4. Assemble BlockerResult:
   - filter: optional<CheckResult>
   - exception: optional<CheckResult>
   - important: bool
   - redirect: optional<string> (data: URL)
   - rewritten_url: optional<string>

5. Final block decision:
   should_block = important || (filter.has_value() && !exception.has_value())
```

### Phase 3: Cosmetic filtering (separate, called for document requests)

```
1. Caller invokes: engine.url_cosmetic_resources(url)
   - Build a Request from the URL
   - Check generichide: blocker.check_generic_hide(request) → bool

2. CosmeticFilterCache::hostname_cosmetic_resources(hostname, generichide):
   2a. Compute candidate hashes:
       - request_hostnames = hashes of [domain, parent, grandparent, ...]
       - request_entities  = hashes of entity labels (last label dropped)

   2b. First pass: gather positives
       For each hash in chain(entities, hostnames):
         - Look up in hostname_hide multimap → add to specific_hide
         - Look up in hostname_inject_script multimap → add to script_injections
         - Look up in specific_rules HashMap → add procedural_actions

   2c. Second pass: apply exceptions
       For each hash in chain(entities, hostnames):
         - Look up in specific_rules for unhide/uninject/procedural_exception
         - Remove from positive sets
         - Accumulate into exceptions set

   2d. Merge with generic selectors (unless generichide):
       - Add misc_generic_selectors (minus exceptions) to hide_selectors

   2e. Resolve scriptlets:
       - For each (script_name, permission) in script_injections:
         - Look up resource, wrap in try/catch, concatenate with dependencies

   2f. Return UrlSpecificResources {
          hide_selectors,
          procedural_actions,   // JSON strings
          exceptions,           // for hidden_class_id_selectors
          injected_script,     // JS string
          generichide,
       }

3. Browser content script:
   - Inject CSS: join hide_selectors with "," + "{display:none!important}"
   - Inject script: injected_script
   - For procedural_actions: deserialize JSON, execute against DOM
   - For dynamic content (MutationObserver): call hidden_class_id_selectors
     with new classes/ids + cached exceptions set
```

---

## Summary for the C++ Rewrite

Here's the cheat sheet of what you need to build, in priority order:

| Priority | Component | C++ equivalent |
|---|---|---|
| 1 | SeaHash (or any stable 64-bit hash) | Implement faithfully if you want `.dat` compat |
| 2 | Tokenizer (alphanumeric runs, len>1, drop `*`-adjacent) | ~50 lines |
| 3 | `TokenSelector` with 40 bad-token seed list | ~80 lines |
| 4 | `FlatMultiMap` (sorted `vector<uint32_t>` + parallel `vector<Filter>`) | `std::multimap` works but cache-unfriendly |
| 5 | `NetworkFilter` struct + parser | ~500 lines, biggest single piece |
| 6 | `Request` struct + URL parser | Use a URL parsing lib (e.g. ada) |
| 7 | Per-rule matcher (`check_pattern` dispatch) | ~400 lines, 9 branches |
| 8 | `is_anchored_by_hostname` (label-aligned match) | ~40 lines, tricky |
| 9 | `RegexManager` with RE2 | ~150 lines + RE2 dependency |
| 10 | `Blocker` orchestrator (8-list dispatch) | ~200 lines |
| 11 | `CosmeticFilter` parser | ~400 lines |
| 12 | `CosmeticFilterCache` (5 generic + 3 specific buckets) | ~300 lines |
| 13 | `ResourceStorage` (scriptlets + redirects) | ~200 lines |
| 14 | Optional: flatbuffer serialization | Skip if you don't need `.dat` compat |
| 15 | Optional: optimizer (AnyOf fusion) | ~100 lines, easy win |

**Key algorithmic insights to internalize:**

1. **No bloom filter** — the "fast lookup" is tokenization + sorted-vector binary search. The "filtering" is done by the rarest-token heuristic, not a probabilistic structure.
2. **Token `0` is the catch-all bucket** — always appended to request tokens so generic rules are consulted.
3. **Source-hostname suffixes are hashed** — so `$domain=parent.com` matches requests from `child.parent.com`.
4. **Negation is stored under the same hash with negated kind** — so exceptions come back from the *same* lookup as positives.
5. **Domain lists are `u32` indices into a global table** — memory optimization, requires a hash→index map rebuilt at load time.
6. **Regexes are lazily compiled and LRU-evicted** — keyed by filter identity (flatbuffer offset), not by pattern string.
7. **The 8-list split** is the architectural backbone — understand it before anything else.


This was done using GLM 5.2 in the z.ai website 