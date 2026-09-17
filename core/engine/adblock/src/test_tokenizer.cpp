#include "../include/tokenizer.h"
#include "../include/flat_multimap.h"
#include "../include/network_filter.h"
#include "../include/request.h"
#include "../include/blocker.h"
#include "../include/token_selector.h"
#include <iostream>

int main() {
    using namespace adblock;

    std::string url = "https://ads.example.com/banner/image.jpg";

    std::cout << "Tokenizing: " << url << "\n\n";

    auto tokens = tokenize(url);

    std::cout << "Tokens found: " << tokens.size() << "\n";
    for (auto t : tokens) {
        if (t == 0)
            std::cout << "  [0] → catch-all bucket\n";
        else
            std::cout << "  " << t << "\n";
    }
        // Test TokenSelector
    TokenSelector selector;
    selector.register_token(tokens[0]);
    selector.register_token(tokens[0]); // seen twice
    selector.register_token(tokens[1]); // seen once

    ShortHash best = selector.select_least_used(tokens);
    std::cout << "\nRarest token selected: " << best << "\n";
        // Test FlatMultiMap
    FlatMultiMap<std::string> map;
    map.insert(1234, "rule_one");
    map.insert(5678, "rule_two");
    map.insert(1234, "rule_three"); // same key as rule_one
    map.finalize();

    auto results = map.get(1234);
    std::cout << "\nFlatMultiMap lookup for key 1234:\n";
    for (auto* r : results)
        std::cout << "  " << *r << "\n";

    auto empty = map.get(9999);
    std::cout << "Lookup for missing key 9999: "
              << (empty.empty() ? "empty (correct)" : "wrong") << "\n";
    
        // Test NetworkFilter parser
    std::cout << "\n--- NetworkFilter Parser Tests ---\n";

    struct TestCase { std::string rule; std::string desc; };
    std::vector<TestCase> tests = {
        {"||ads.example.com^",                    "basic hostname anchor"},
        {"@@||example.com^",                      "exception rule"},
        {"||banner*$script,third-party",          "wildcard + options"},
        {"||example.com^$domain=foo.com|~bar.com","domain option"},
        {"! this is a comment",                   "comment (should skip)"},
        {"/ads/banner/$image",                    "left anchor + image"},
    };

    for (auto& tc : tests) {
        NetworkFilter nf;
        bool ok = NetworkFilter::parse(tc.rule, nf);
        std::cout << "\n[" << tc.desc << "]\n";
        std::cout << "  rule:     " << tc.rule << "\n";
        std::cout << "  parsed:   " << (ok ? "yes" : "no (skipped)") << "\n";
        if (ok) {
            std::cout << "  hostname: " << (nf.hostname.empty() ? "(none)" : nf.hostname) << "\n";
            std::cout << "  pattern:  " << (nf.pattern.empty() ? "(none)" : nf.pattern) << "\n";
            std::cout << "  exception:" << (nf.is_exception() ? "yes" : "no") << "\n";
            std::cout << "  important:" << (nf.is_important() ? "yes" : "no") << "\n";
            std::cout << "  regex:    " << (nf.is_regex() ? "yes" : "no") << "\n";
            std::cout << "  domains:  " << nf.opt_domains.size() << " positive, "
                      << nf.opt_not_domains.size() << " negative\n";
        }
    }
        // Test Request
    std::cout << "\n--- Request Builder Tests ---\n";
    auto req = Request::build(
        "https://ads.example.com/banner/image.jpg",
        "https://mysite.com/page",
        RequestType::Image
    );
    std::cout << "hostname:     " << req.hostname << "\n";
    std::cout << "source:       " << req.source_hostname << "\n";
    std::cout << "is_https:     " << (req.is_https ? "yes" : "no") << "\n";
    std::cout << "is_third_party: " << (req.is_third_party ? "yes" : "no") << "\n";
    std::cout << "tokens:       " << req.tokens.size() << "\n";
    std::cout << "source_hashes:" << req.source_hashes.size() << "\n";
        // Test Blocker — the full engine
    std::cout << "\n--- Blocker Engine Tests ---\n";

    Blocker blocker;
    blocker.load(R"(
||ads.example.com^
||banner.com^$third-party
@@||safe.example.com^
||important-block.com^$important
! this is a comment
||google-analytics.com^$script,third-party
)");
    blocker.finalize();

    struct UrlTest {
        std::string url;
        std::string source;
        RequestType type;
        std::string desc;
    };

    std::vector<UrlTest> url_tests = {
        {"https://ads.example.com/track.js",   "https://mysite.com",    RequestType::Script,  "should BLOCK (ads)"},
        {"https://safe.example.com/img.jpg",   "https://mysite.com",    RequestType::Image,   "should ALLOW (exception)"},
        {"https://important-block.com/x",      "https://mysite.com",    RequestType::Other,   "should BLOCK (important)"},
        {"https://google-analytics.com/ga.js", "https://mysite.com",    RequestType::Script,  "should BLOCK (analytics)"},
        {"https://clean.com/page",             "https://mysite.com",    RequestType::Document,"should ALLOW (no rule)"},
    };

    for (auto& t : url_tests) {
        auto req = Request::build(t.url, t.source, t.type);
        auto res = blocker.check(req);
        std::cout << "\n[" << t.desc << "]\n";
        std::cout << "  url:         " << t.url << "\n";
        std::cout << "  blocked:     " << (res.should_block ? "YES" : "NO") << "\n";
        std::cout << "  important:   " << (res.is_important ? "yes" : "no") << "\n";
        std::cout << "  exception:   " << (res.is_exception ? "yes" : "no") << "\n";
        if (!res.matched_rule.empty())
            std::cout << "  matched:     " << res.matched_rule << "\n";
    }
    return 0;
} 

// this runs and is sucess full it brings the following terminal responce after running Moonbug@peaseernest:~/dev/nothing/packages/adblock$ g++ -std=c++17 src/tokenizer.cpp src/test_tokenizer.cpp -o test_tokenizer && ./test_tokenizer  -- this builds the tokenizer and runs it i am in linux windows might be way way harder to just run one line the output is Tokenizing: https://ads.example.com/banner/image.jpg

// Tokens found: 8
//   2323990411
//   1069088285
//   2810081614
//   3338110467
//   3535605788
//   1424881944
//   3733967210
//   [0] → catch-all bucket
 // this is a sucess 