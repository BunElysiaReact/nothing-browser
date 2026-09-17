#include "Interceptor.h"
#include "request.h"
#include "blocker.h"
#include <QDateTime>
#include <QFile>
#include <QRegularExpression>

// ── Constructor ───────────────────────────────────────────────────────────────
// Loads EasyList + EasyPrivacy from Qt resources and finalizes the blocker.
// These are shipped as compiled-in assets (:/filters/...) so no disk access
// is needed at runtime — the blocker is hot and ready to go from line one.
Interceptor::Interceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    QString combined;
    for (const QString &path : {":/filters/easylist.txt", ":/filters/easyprivacy.txt"}) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            combined += f.readAll() + "\n";
    }
    m_blocker.load(combined.toStdString());
    m_blocker.finalize();
}

// ── toAdblockType ─────────────────────────────────────────────────────────────
// Maps Qt's resource-type integers to the adblock library's RequestType enum.
// Anything Qt doesn't have a specific case for becomes "Other" — the blocker
// will match it against generic filters rather than type-specific ones.
adblock::RequestType Interceptor::toAdblockType(int rt)
{
    using RT = QWebEngineUrlRequestInfo;
    switch (rt) {
        case RT::ResourceTypeImage:        return adblock::RequestType::Image;
        case RT::ResourceTypeScript:       return adblock::RequestType::Script;
        case RT::ResourceTypeStylesheet:   return adblock::RequestType::Stylesheet;
        case RT::ResourceTypeXhr:          return adblock::RequestType::XmlHttpRequest;
        case RT::ResourceTypeFontResource: return adblock::RequestType::Font;
        case RT::ResourceTypeMedia:        return adblock::RequestType::Media;
        case RT::ResourceTypeSubFrame:     return adblock::RequestType::Subdocument;
        case RT::ResourceTypeMainFrame:    return adblock::RequestType::Document;
        default:                           return adblock::RequestType::Other;
    }
}

// ── interceptRequest ──────────────────────────────────────────────────────────
// Called by Chromium for every outgoing network request.  Pipeline:
//   1. Redirect trap  — kills known ad-redirect domains & obfuscated URLs
//   2. Adblock        — EasyList/EasyPrivacy filter check
//   3. Header spoof   — User-Agent, Client Hints, Sec-Fetch-* etc.
//   4. Debug emit     — requestSeen + requestIntercepted signals
void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    using RT = QWebEngineUrlRequestInfo;
    const QString url  = info.requestUrl().toString();
    const auto    type = info.resourceType();

    // ── 1. Redirect trap ──────────────────────────────────────────────────────
    // Catches frame-level navigations only (main frame + sub-frames).
    // We don't bother with sub-resources here — the adblock engine handles those.
    if (type == RT::ResourceTypeMainFrame || type == RT::ResourceTypeSubFrame) {

        // Hard-coded domain blocklist — fast path before any regex work.
        static const QStringList adRedirectDomains = {
            "moonlighthathel.org",
            "doubleclick.net",
            "ad.doubleclick.net",
            "googleadservices.com",
            "outbrain.com",
            "taboola.com",
            "revcontent.com",
            "bidswitch.net",
            "rubiconproject.com",
            "openx.net",
        };
        const QString host = info.requestUrl().host();
        for (const QString &domain : adRedirectDomains) {
            if (host.endsWith(domain)) {
                info.block(true);
                return;
            }
        }

        // Heuristic: URLs with >15 percent-encoded chars AND a long base64-ish
        // token are almost always tracker redirect chains. Block them.
        static const QRegularExpression redirectPattern(R"([A-Za-z0-9+/]{30,}={0,2})");
        if (url.count('%') > 15 && redirectPattern.match(url).hasMatch()) {
            info.block(true);
            return;
        }
    }

    // ── 2. Adblock (EasyList / EasyPrivacy) ──────────────────────────────────
    if (m_adblockEnabled) {
        auto req = adblock::Request::build(
            url.toStdString(),
            info.firstPartyUrl().toString().toStdString(),
            toAdblockType(static_cast<int>(type))
        );
        if (m_blocker.check(req).should_block) {
            info.block(true);
            return;
        }
    }

    // ── 3. Header spoofing ────────────────────────────────────────────────────
    // Everything below only runs on requests that survived the blockers.

    // User-Agent — presents as a stock Chrome 124 on Linux.
    info.setHttpHeader("User-Agent",
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36");

    // Client Hints — must match the UA string above or servers get suspicious.
    info.setHttpHeader("Sec-CH-UA",
        "\"Google Chrome\";v=\"124\", "
        "\"Chromium\";v=\"124\", "
        "\"Not-A.Brand\";v=\"99\"");
    info.setHttpHeader("Sec-CH-UA-Mobile",   "?0");
    info.setHttpHeader("Sec-CH-UA-Platform", "\"Linux\"");

    // Standard negotiation headers.
    info.setHttpHeader("Accept-Language", "en-US,en;q=0.9");
    info.setHttpHeader("Accept-Encoding", "gzip, deflate, br");

    // Sec-Fetch-Dest — maps resource type to the correct fetch destination token.
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

    // Sec-Fetch-Mode / Sec-Fetch-User / Sec-Fetch-Site — FRAME NAVIGATIONS ONLY.
    //
    // DO NOT set these for sub-resources under any circumstances.
    // Even an empty-string setHttpHeader() emits the header, which confuses
    // CORS preflight on cross-origin resources (e.g. CDN assets, API calls).
    // Let Qt/Chromium fill them in natively for everything that isn't a frame.
    if (type == RT::ResourceTypeMainFrame || type == RT::ResourceTypeSubFrame) {
        info.setHttpHeader("Sec-Fetch-Mode", "navigate");
        info.setHttpHeader("Sec-Fetch-User", "?1");
        info.setHttpHeader("Sec-Fetch-Site", "none");
    }

    // ── 4. Debug signals ──────────────────────────────────────────────────────
    // requestSeen  — every surviving request (raw debug view).
    // requestIntercepted — every surviving non-XHR request with a resolved type
    //                      label (used by the request inspector UI).

    static const QStringList typeNames = {
        "Document", "Link", "Image", "StyleSheet", "Script",
        "FontResource", "SubResource", "Object", "Media",
        "Worker", "SharedWorker", "Prefetch", "Favicon",
        "XHR", "Ping", "ServiceWorker", "CSPReport", "PluginResource",
        "NavigationPreload", "Other"
    };

    const QString method   = QString::fromLatin1(info.requestMethod());
    const QString typeName = typeNames.value(static_cast<int>(type), "Other");

    // Raw debug signal — always emitted.
    const QString debugHeaders =
        QString("Method: %1\nURL: %2\nType: %3\nTime: %4")
            .arg(method).arg(url)
            .arg(static_cast<int>(type))
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    emit requestSeen(method, url, debugHeaders);

    // Inspector signal — skip XHR to avoid flooding the UI with API noise.
    if (typeName != "XHR") {
        emit requestIntercepted(method, url, typeName, "{}");
    }
}