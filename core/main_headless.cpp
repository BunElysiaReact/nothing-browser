#include <QApplication>
#include <QWebEngineProfile>
#include "engine/PiggyServer.h"

int main(int argc, char *argv[]) {
    // Must be set BEFORE QApplication
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-gpu "
        "--no-sandbox "
        "--disable-dev-shm-usage "
        "--disable-software-rasterizer "
        "--headless=new "
        "--allow-running-insecure-content"
    );
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);  // NOT QCoreApplication
    app.setApplicationName("nothing-browser-headless");

    PiggyServer server(nullptr);
    server.start();

    qDebug() << "[Piggy] Headless daemon on socket: piggy";
    return app.exec();
}