#include "RequestInterceptor.h"
#include "request.h"
#include "blocker.h"
#include <QFile>
#include <QRegularExpression>

RequestInterceptor::RequestInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {
    QString combined;
    // Load EasyList - general ad blocking filters
    QFile easylist(":/filters/easylist.txt");
    if (easylist.open(QIODevice::ReadOnly))
        combined += easylist.readAll() + "\n";
    
    // Load EasyPrivacy - tracking protection filters
    QFile easyprivacy(":/filters/easyprivacy.txt");
    if (easyprivacy.open(QIODevice::ReadOnly))
        combined += easyprivacy.readAll() + "\n";
    
    // Load custom anti-adblock filters for enhanced blocking
    QFile custom(":/filters/custom-anti-adblock.txt");
    if (custom.open(QIODevice::ReadOnly))
        combined += custom.readAll() + "\n";
    
    m_blocker.load(combined.toStdString());
    m_blocker.finalize();
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    // ── Redirect trap ─────────────────────────────────────────────────────
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame ||
        info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeSubFrame) {
        QString url = info.requestUrl().toString();
        static const QRegularExpression redirectPattern(
            R"([A-Za-z0-9+/]{30,}={0,2})"
        );
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
        QString host = info.requestUrl().host();
        for (const QString &domain : adRedirectDomains) {
            if (host.endsWith(domain)) {
                info.block(true);
                return;
            }
        }
        if (url.count('%') > 15 && redirectPattern.match(url).hasMatch()) {
            info.block(true);
            return;
        }
    }

    // ── Adblock ───────────────────────────────────────────────────────────
    auto toAdblockType = [](int rt) -> adblock::RequestType {
        switch (rt) {
            case QWebEngineUrlRequestInfo::ResourceTypeImage:        return adblock::RequestType::Image;
            case QWebEngineUrlRequestInfo::ResourceTypeScript:       return adblock::RequestType::Script;
            case QWebEngineUrlRequestInfo::ResourceTypeStylesheet:   return adblock::RequestType::Stylesheet;
            case QWebEngineUrlRequestInfo::ResourceTypeXhr:          return adblock::RequestType::XmlHttpRequest;
            case QWebEngineUrlRequestInfo::ResourceTypeFontResource: return adblock::RequestType::Font;
            case QWebEngineUrlRequestInfo::ResourceTypeMedia:        return adblock::RequestType::Media;
            case QWebEngineUrlRequestInfo::ResourceTypeSubFrame:     return adblock::RequestType::Subdocument;
            case QWebEngineUrlRequestInfo::ResourceTypeMainFrame:    return adblock::RequestType::Document;
            default:                                                 return adblock::RequestType::Other;
        }
    };

    if (m_adblockEnabled) {
        std::string urlStr = info.requestUrl().toString().toStdString();
        std::string firstPartyStr = info.firstPartyUrl().toString().toStdString();
        
        auto req = adblock::Request::build(urlStr, firstPartyStr, toAdblockType(info.resourceType()));
        auto result = m_blocker.check(req);
        
        if (result.should_block) {
            info.block(true);
            return;
        }
    }

    // ── Capture ───────────────────────────────────────────────────────────
    static const QStringList types = {
        "Document","Link","Image","StyleSheet","Script",
        "FontResource","SubResource","Object","Media",
        "Worker","SharedWorker","Prefetch","Favicon",
        "XHR","Ping","ServiceWorker","CSPReport","PluginResource",
        "NavigationPreload","Other"
    };

    QString type = types.value((int)info.resourceType(), "Other");
    if (type == "XHR") return;

    emit requestIntercepted(
        QString::fromUtf8(info.requestMethod()),
        info.requestUrl().toString(),
        type,
        "{}"
    );
}