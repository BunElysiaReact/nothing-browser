#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>
#include <optional>

namespace adblock {

enum class RequestType {
    Image,
    Media,
    Object,
    Other,
    Ping,
    Script,
    Stylesheet,
    Subdocument,
    Websocket,
    XmlHttpRequest,
    Font,
    Document,
};

struct Request {
    std::string url;
    std::string url_lower;
    std::string hostname;
    std::string source_hostname;
    RequestType type;
    bool is_http        = false;
    bool is_https       = false;
    bool is_third_party = false;

    // Precomputed tokens for fast lookup
    std::vector<ShortHash> tokens;

    // Source hostname + all parent suffixes hashed
    // e.g. "a.b.example.com" → [hash("a.b.example.com"),
    //                           hash("b.example.com"),
    //                           hash("example.com"),
    //                           hash("com")]
    std::vector<Hash> source_hashes;

    // Build a Request from a URL, source URL and type
    static Request build(
        const std::string& url,
        const std::string& source_url,
        RequestType type
    );

private:
    static std::string extract_hostname(const std::string& url);
    static std::string get_etld_plus1(const std::string& hostname);
    static std::vector<Hash> compute_source_hashes(const std::string& hostname);
};

} // namespace adblock
