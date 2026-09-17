# nothing-adblock

> A from-scratch C++ adblock engine built for the Nothing ecosystem.
> No dependencies. No Rust. No bloat.

---

## How it works

nothing-adblock is a network request filter engine written in C++17.
It parses standard EasyList/uBlock Origin filter syntax and evaluates
incoming URLs against those rules in microseconds.

The engine is built around five core components:

- **Tokenizer** — splits URLs into hash tokens for fast lookup
- **TokenSelector** — picks the rarest token per rule to minimize bucket size
- **FlatMultiMap** — sorted vector with binary search, the lookup backbone
- **NetworkFilter** — parses filter list rules into matchable structs
- **Blocker** — orchestrates everything, returns block/allow decisions

---

## Using nothing-adblock in your project

### 1. Include the headers

```cpp
#include "include/blocker.h"
#include "include/request.h"
```

### 2. Load your filter list

```cpp
adblock::Blocker blocker;

// Load from a string (EasyList format)
blocker.load(R"(
||ads.example.com^
||google-analytics.com^$script,third-party
@@||safe.example.com^
||important-block.com^$important
)");

// Must call finalize before checking requests
blocker.finalize();
```

### 3. Check a request

```cpp
auto req = adblock::Request::build(
    "https://ads.example.com/track.js",  // URL to check
    "https://mysite.com/page",           // source page URL
    adblock::RequestType::Script         // request type
);

auto result = blocker.check(req);

if (result.should_block) {
    // Block the request
    // result.matched_rule contains the rule that matched
    // result.is_important is true if $important rule matched
} else {
    // Allow the request
    // result.is_exception is true if an @@ rule matched
}
```

### 4. Request types

```cpp
adblock::RequestType::Image
adblock::RequestType::Script
adblock::RequestType::Stylesheet
adblock::RequestType::XmlHttpRequest
adblock::RequestType::Font
adblock::RequestType::Media
adblock::RequestType::Websocket
adblock::RequestType::Subdocument
adblock::RequestType::Document
adblock::RequestType::Other
```

---

## Filter list syntax supported

| Syntax | Example | What it does |
|--------|---------|--------------|
| Hostname anchor | `\|\|ads.com^` | Block any request to ads.com |
| Exception | `@@\|\|safe.com^` | Always allow safe.com |
| Important | `\|\|block.com^$important` | Block even if exception exists |
| Third party | `\|\|track.com^$third-party` | Only block cross-site requests |
| Content type | `\|\|ads.com^$script,image` | Only block scripts and images |
| Domain scope | `\|\|ads.com^$domain=foo.com` | Only block when on foo.com |
| Domain exclude | `\|\|ads.com^$domain=~bar.com` | Block everywhere except bar.com |
| Left anchor | `\|https://ads.com` | URL must start with this |
| Right anchor | `ads-banner\|` | URL must end with this |
| Wildcard | `\|\|ads*.com^` | * matches anything |
| Comment | `! this is ignored` | Comments start with ! |

---

## How the Nothing ecosystem uses it

Every browser in the Nothing ecosystem plugs into nothing-adblock
as a shared C++ library. The engine runs once per network request,
before the page even starts loading.

### Nothing Browser (scraper)
- Adblock is **optional and toggleable** per session
- Useful when scraping pages that serve different content to ad blockers
- Network capture still works even when adblock is active

### Nothing Private Browser
- Adblock is **always on**, no toggle
- Combined with fingerprint spoofing and WebRTC leak protection
- Blocks ads, trackers, and analytics scripts by default
- Ships with EasyList + EasyPrivacy preloaded

### Sabre Browser
- Adblock is **on by default**, toggleable in settings
- Ships with EasyList + uBlock Origin filter lists preloaded
- Powers the native blocking pipeline — evaluated before
  QtWebEngine even sees the request, so zero rendering overhead
- Users can add custom filter lists from settings

### Shared filter list pipeline

All three browsers share the same filter list update mechanism:

```
Filter list URL (EasyList CDN)
        ↓
  Download & parse
        ↓
  nothing-adblock engine
        ↓
  Block decision returned to Qt network interceptor
        ↓
  Request blocked or allowed before page load
```

---

## Roadmap

| Feature | Status |
|---------|--------|
| Core engine (tokenizer, parser, blocker) | ✅ Done |
| EasyList/uBlock syntax support | ✅ Done |
| Exception rules (`@@`) | ✅ Done |
| `$important` modifier | ✅ Done |
| `$domain=` option | ✅ Done |
| Content type filtering | ✅ Done |
| Regex filter support (`/regex/`) | 🔨 Next |
| Cosmetic filtering (hide DOM elements) | 📋 Planned |
| Scriptlet injection | 📋 Planned |
| Filter list auto-update | 📋 Planned |
| `$redirect=` support | 📋 Planned |
| `$removeparam=` support | 📋 Planned |
| Qt network interceptor integration | 📋 Planned |

---

*Part of the Nothing ecosystem · Built by Ernest Tech House · Kenya 🇰🇪*
