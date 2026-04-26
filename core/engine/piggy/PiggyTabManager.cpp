#include "PiggyServer.h"
#include "../../tabs/PiggyTab.h"
#include "../NetworkCapture.h"
#include "../Interceptor.h"
#include "../FingerprintSpoofer.h"
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QDir>
#include <QSet>
#include <QUuid>

// ─── Profile configuration ────────────────────────────────────────────────────

void piggy_configureProfile(QWebEngineProfile *profile) {
    QString base = QDir::homePath() + "/.piggy/" + profile->storageName();
    profile->setPersistentStoragePath(base + "/storage");
    profile->setCachePath(base + "/cache");
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    profile->setHttpUserAgent(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36"
    );

    auto *s = profile->settings();
    s->setAttribute(QWebEngineSettings::LocalStorageEnabled,             true);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    s->setAttribute(QWebEngineSettings::AllowRunningInsecureContent,     true);
    s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows,        true);
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard,    true);
    s->setAttribute(QWebEngineSettings::WebGLEnabled,                    true);
    s->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled,      true);
    s->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled,           true);

    static const QString kChromeSpoof = R"JS(
(function() {
    Object.defineProperty(navigator, 'webdriver', { get: () => false, configurable: true });
    Object.defineProperty(navigator, 'plugins', {
        get: () => { var arr=[{name:'Chrome PDF Plugin'},{name:'Chrome PDF Viewer'},{name:'Native Client'}]; arr.item=i=>arr[i]; arr.refresh=()=>{}; arr.namedItem=n=>arr.find(p=>p.name===n)||null; return arr; },
        configurable: true
    });
    Object.defineProperty(navigator, 'languages', { get: () => ['en-US','en'], configurable: true });
    Object.defineProperty(navigator, 'platform',  { get: () => 'Win32', configurable: true });
    Object.defineProperty(navigator, 'userAgent', {
        get: () => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
        configurable: true
    });
    Object.defineProperty(navigator, 'userAgentData', {
        get: () => ({
            brands: [
                { brand: 'Chromium',      version: '124' },
                { brand: 'Google Chrome', version: '124' },
                { brand: 'Not-A.Brand',   version: '99'  }
            ],
            mobile: false,
            platform: 'Windows',
            getHighEntropyValues: function(hints) {
                return Promise.resolve({
                    platform: 'Windows', platformVersion: '10.0.0',
                    architecture: 'x86', bitness: '64', model: '',
                    uaFullVersion: '124.0.0.0',
                    fullVersionList: [
                        { brand: 'Chromium',      version: '124.0.0.0' },
                        { brand: 'Google Chrome', version: '124.0.0.0' },
                        { brand: 'Not-A.Brand',   version: '99.0.0.0'  }
                    ]
                });
            }
        }),
        configurable: true
    });
    if (!window.chrome) {
        window.chrome = {
            runtime: {
                onMessage:   { addListener: function(){}, removeListener: function(){} },
                sendMessage: function(){},
                connect:     function(){ return { onMessage:{addListener:function(){}}, postMessage:function(){}, disconnect:function(){} }; },
                onConnect:   { addListener: function(){} },
                id: undefined
            },
            loadTimes: function(){ return {}; },
            csi:       function(){ return {}; }
        };
    }
    if (!navigator.permissions) {
        Object.defineProperty(navigator, 'permissions', {
            get: () => ({ query: function(p){ return Promise.resolve({ state:'granted', onchange:null }); } }),
            configurable: true
        });
    }
    if (navigator.storage) {
        navigator.storage.persist   = function(){ return Promise.resolve(true); };
        navigator.storage.persisted = function(){ return Promise.resolve(true); };
    }
    if (window.Notification) {
        Object.defineProperty(Notification, 'permission', { get: () => 'default', configurable: true });
    }
})();
)JS";

    QWebEngineScript spoofChrome;
    spoofChrome.setName("nothing_chrome_spoof_global");
    spoofChrome.setSourceCode(kChromeSpoof);
    spoofChrome.setInjectionPoint(QWebEngineScript::DocumentCreation);
    spoofChrome.setWorldId(QWebEngineScript::MainWorld);
    spoofChrome.setRunsOnSubFrames(true);
    profile->scripts()->insert(spoofChrome);
}

// ─── page() ──────────────────────────────────────────────────────────────────

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId) {
    if (!tabId.isEmpty() && tabId != "default") {
        auto it = srv->tabs().find(tabId);
        if (it != srv->tabs().end()) return it.value().page;
        qWarning() << "[PiggyServer] Unknown tabId:" << tabId << "— falling back to default";
    }
    if (srv->piggy())       return srv->piggy()->getPage();
    if (srv->headfulPage()) return srv->headfulPage();
    return srv->ownPage();
}

// ─── createTab() ─────────────────────────────────────────────────────────────

QString piggy_createTab(PiggyServer *srv) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QWebEngineProfile *profile = nullptr;
    if (srv->piggy())
        profile = srv->piggy()->getPage()->profile();
    else if (srv->headfulPage())
        profile = srv->headfulPage()->profile();
    else
        profile = srv->ownProfile();

    static QSet<QWebEngineProfile*> s_configured;
    if (!s_configured.contains(profile)) {
        piggy_configureProfile(profile);
        s_configured.insert(profile);
    }

    auto *p = new QWebEnginePage(profile, srv);

    // Fingerprint spoof
    auto &spoofer = FingerprintSpoofer::instance();
    QWebEngineScript spoofScript;
    spoofScript.setName("nothing_fingerprint_" + id);
    spoofScript.setSourceCode(spoofer.injectionScript());
    spoofScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    spoofScript.setWorldId(QWebEngineScript::MainWorld);
    spoofScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(spoofScript);

    // Capture script
    QWebEngineScript capScript;
    capScript.setName("nothing_capture_" + id);
    capScript.setSourceCode(NetworkCapture::captureScript());
    capScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    capScript.setWorldId(QWebEngineScript::MainWorld);
    capScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(capScript);

    auto *interceptor = new Interceptor(srv);
    profile->setUrlRequestInterceptor(interceptor);

    auto *capture = new NetworkCapture(srv);
    capture->attachToPage(p, profile);

    QObject::connect(capture, &NetworkCapture::requestCaptured, srv,
        [srv, id](const CapturedRequest &req) {
            srv->onRequestCaptured(req, id);
        });
    QObject::connect(capture, &NetworkCapture::wsFrameCaptured, srv,
        [srv, id](const WebSocketFrame &frame) {
            srv->onWsFrameCaptured(frame, id);
        });
    QObject::connect(capture, &NetworkCapture::cookieCaptured, srv,
        [srv, id](const CapturedCookie &cookie) {
            srv->onCookieCaptured(cookie, id);
        });
    QObject::connect(capture, &NetworkCapture::cookieRemoved, srv,
        [srv, id](const QString &name, const QString &domain) {
            srv->onCookieRemoved(name, domain, id);
        });
    QObject::connect(capture, &NetworkCapture::storageCaptured, srv,
        [srv, id](const QString &origin, const QString &key,
                  const QString &value, const QString &type) {
            srv->onStorageCaptured(origin, key, value, type, id);
        });
    QObject::connect(p, &QWebEnginePage::urlChanged, srv,
        [srv, id](const QUrl &url) {
            QJsonObject event;
            event["type"]  = "event";
            event["event"] = "navigate";
            event["tabId"] = id;
            event["url"]   = url.toString();
            QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
            for (auto *client : srv->clients()) {
                if (client && client->state() == QLocalSocket::ConnectedState)
                    client->write(msg);
            }
        });

    TabContext ctx;
    ctx.page        = p;
    ctx.interceptor = interceptor;
    ctx.capture     = capture;
    srv->tabs().insert(id, ctx);

    emit srv->tabCreated(id, p);
    qDebug() << "[PiggyServer] Tab created:" << id;
    return id;
}

// ─── closeTab() ──────────────────────────────────────────────────────────────

void piggy_closeTab(PiggyServer *srv, const QString &tabId) {
    if (!srv->tabs().contains(tabId)) return;
    TabContext &ctx = srv->tabs()[tabId];
    ctx.page->deleteLater();
    ctx.interceptor->deleteLater();
    ctx.capture->deleteLater();
    srv->tabs().remove(tabId);
    emit srv->tabClosed(tabId);
    qDebug() << "[PiggyServer] Tab closed:" << tabId;
}