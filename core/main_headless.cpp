// core/main_headless.cpp
#include <QApplication>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include "engine/PiggyServer.h"

int main(int argc, char *argv[]) {
    // FIX: added --disable-dev-shm-usage       → prevents shm SIGSEGV on low-RAM machines
    //      added --disable-site-isolation-trials → disables COOP renderer isolation
    //        that crashes on ChatGPT (and other sites with COOP/COEP headers)
    //      added --disable-features=IsolateOrigins → same family of fixes
    //      added --js-flags                     → cap renderer heap for i3/8GB
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-gpu "
        "--no-sandbox "
        "--disable-dev-shm-usage "
        "--disable-software-rasterizer "
        "--headless=new "
        "--allow-running-insecure-content "
        "--disable-site-isolation-trials "
        "--disable-features=IsolateOrigins,WebRtcHideLocalIpsWithMdns "
        "--js-flags=--max-old-space-size=512"
    );
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    app.setApplicationName("nothing-browser-headless");

    // FIX: was PiggyServer(static_cast<PiggyTab*>(nullptr)) which uses the
    //      PiggyTab* constructor — correct overload is QWebEnginePage* so
    //      m_headfulPage is set and page() resolution is unambiguous.
    auto *profile = new QWebEngineProfile(&app);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    auto *defaultPage = new QWebEnginePage(profile, &app);

    PiggyServer server(defaultPage, &app);
    server.start();

    qDebug() << "[Piggy] Headless daemon on socket: piggy";
    return app.exec();
}