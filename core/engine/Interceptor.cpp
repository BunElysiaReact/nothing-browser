#include "Interceptor.h"
#include <QDateTime>

Interceptor::Interceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {}

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    QString url = info.requestUrl().toString();
    auto type = info.resourceType();
    using RT = QWebEngineUrlRequestInfo;

    // ── User-Agent ────────────────────────────────────────────────────────────
    info.setHttpHeader("User-Agent",
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36");

    // ── Client Hints ──────────────────────────────────────────────────────────
    info.setHttpHeader("Sec-CH-UA",
        "\"Google Chrome\";v=\"124\", "
        "\"Chromium\";v=\"124\", "
        "\"Not-A.Brand\";v=\"99\"");
    info.setHttpHeader("Sec-CH-UA-Mobile",   "?0");
    info.setHttpHeader("Sec-CH-UA-Platform", "\"Linux\"");

    // ── Standard headers ──────────────────────────────────────────────────────
    info.setHttpHeader("Accept-Language", "en-US,en;q=0.9");
    info.setHttpHeader("Accept-Encoding", "gzip, deflate, br");

    // ── Sec-Fetch-Dest ────────────────────────────────────────────────────────
    QString dest;
    switch (type) {
        case RT::ResourceTypeMainFrame:  dest = "document"; break;
        case RT::ResourceTypeSubFrame:   dest = "iframe";   break;
        case RT::ResourceTypeScript:     dest = "script";   break;
        case RT::ResourceTypeStylesheet: dest = "style";    break;
        case RT::ResourceTypeImage:      dest = "image";    break;
        case RT::ResourceTypeXhr:        dest = "empty";    break;
        case RT::ResourceTypeMedia:      dest = "video";    break;
        default:                         dest = "empty";    break;
    }
    info.setHttpHeader("Sec-Fetch-Dest", dest.toUtf8());

    // ── Sec-Fetch-Mode ────────────────────────────────────────────────────────
    // DO NOT touch Sec-Fetch-Mode or fetch-mode for non-frame requests AT ALL.
    // Setting them to empty string still sends the header and triggers CORS.
    // Only set for frame navigations where we know the correct value.
    if (type == RT::ResourceTypeMainFrame || type == RT::ResourceTypeSubFrame) {
        info.setHttpHeader("Sec-Fetch-Mode", "navigate");
        info.setHttpHeader("Sec-Fetch-User", "?1");
        info.setHttpHeader("Sec-Fetch-Site", "none");
    }
    // For ALL other resource types: touch NOTHING — let Qt/Chromium set
    // Sec-Fetch-Mode, Sec-Fetch-Site, Sec-Fetch-User natively.
    // Any setHttpHeader() call here, even with empty value, adds the header
    // to the request and breaks CORS preflight on cross-origin resources.

    // ── Debug emit ────────────────────────────────────────────────────────────
    QString method = QString::fromLatin1(info.requestMethod());
    QString headers = QString("Method: %1\nURL: %2\nType: %3\nTime: %4")
                        .arg(method).arg(url)
                        .arg(static_cast<int>(type))
                        .arg(QDateTime::currentDateTime()
                                 .toString("hh:mm:ss.zzz"));
    emit requestSeen(method, url, headers);
}