// core/main_headless.cpp
#include <QApplication>
#include <QWebEngineProfile>
#include "engine/PiggyServer.h"

int main(int argc, char *argv[]) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-gpu "
        "--no-sandbox "
        "--disable-dev-shm-usage "
        "--disable-software-rasterizer "
        "--headless=new "
        "--allow-running-insecure-content"
    );
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    app.setApplicationName("nothing-browser-headless");

    PiggyServer server(static_cast<PiggyTab*>(nullptr));  // <-- cast fixes ambiguity
    server.start();

    qDebug() << "[Piggy] Headless daemon on socket: piggy";
    return app.exec();
}