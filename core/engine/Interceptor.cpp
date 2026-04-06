#include "Interceptor.h"
#include <QDateTime>

Interceptor::Interceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {}

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    QString url = info.requestUrl().toString();

    // ── User-Agent ────────────────────────────────────────────────────────────
    info.setHttpHeader("User-Agent",
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36");

    // ── Client Hints — FIX: Chrome 110+ brand format ──────────────────────────
    // Old wrong format: "Not(A:Brand" — broke consistency checks
    // New correct format: "Google Chrome" first, then "Not-A.Brand"
    info.setHttpHeader("Sec-CH-UA",
        "\"Google Chrome\";v=\"124\", "
        "\"Chromium\";v=\"124\", "
        "\"Not-A.Brand\";v=\"99\"");
    info.setHttpHeader("Sec-CH-UA-Mobile",    "?0");
    info.setHttpHeader("Sec-CH-UA-Platform",  "\"Linux\"");

    // ── Standard headers ──────────────────────────────────────────────────────
    info.setHttpHeader("Accept-Language", "en-US,en;q=0.9");
    info.setHttpHeader("Accept-Encoding", "gzip, deflate, br");

    // ── Sec-Fetch headers ─────────────────────────────────────────────────────
    auto type = info.resourceType();
    using RT = QWebEngineUrlRequestInfo;

    QString dest;
    switch (type) {
        case RT::ResourceTypeMainFrame:    dest = "document"; break;
        case RT::ResourceTypeSubFrame:     dest = "iframe";   break;
        case RT::ResourceTypeScript:       dest = "script";   break;
        case RT::ResourceTypeStylesheet:   dest = "style";    break;
        case RT::ResourceTypeImage:        dest = "image";    break;
        case RT::ResourceTypeXhr:          dest = "empty";    break;
        case RT::ResourceTypeMedia:        dest = "video";    break;
        default:                           dest = "empty";    break;
    }

    info.setHttpHeader("Sec-Fetch-Dest", dest.toUtf8());
    info.setHttpHeader("Fetch-Mode",
        (type == RT::ResourceTypeMainFrame || type == RT::ResourceTypeSubFrame)
            ? "navigate" : "no-cors");
    info.setHttpHeader("Sec-Fetch-Site", "cross-site");
    info.setHttpHeader("Sec-Fetch-User",
        (type == RT::ResourceTypeMainFrame) ? "?1" : "");

    QString method  = QString::fromLatin1(info.requestMethod());
    QString headers = QString("Method: %1\nURL: %2\nType: %3\nTime: %4")
                        .arg(method).arg(url)
                        .arg(static_cast<int>(type))
                        .arg(QDateTime::currentDateTime()
                                 .toString("hh:mm:ss.zzz"));
    emit requestSeen(method, url, headers);
}