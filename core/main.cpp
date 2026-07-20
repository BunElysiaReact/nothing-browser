#include <QApplication>
#include <QWebEngineProfile>
#include <QTimer>
#include "app/MainWindow.h"
#include "CdpProbe.h"   // <-- new

int main(int argc, char *argv[]) {
    // ── Chromium flags — set BEFORE QApplication ──────────────────────────────
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-blink-features=AutomationControlled "
        "--no-sandbox "
        "--allow-running-insecure-content "
        "--disable-dev-shm-usage "
        "--disable-site-isolation-trials "
        "--disable-features=IsolateOrigins,WebRtcHideLocalIpsWithMdns "
        "--js-flags=--max-old-space-size=512"
    );

    // ── NEW: enable remote debugging on port 9222 ──────────────────────────
    qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "127.0.0.1:9222");

    QApplication app(argc, argv);
    app.setApplicationName("Nothing Browser");
    app.setApplicationVersion("0.1.0");

    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(18, 18, 18));
    dark.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    dark.setColor(QPalette::Base,            QColor(24, 24, 24));
    dark.setColor(QPalette::AlternateBase,   QColor(30, 30, 30));
    dark.setColor(QPalette::Text,            QColor(220, 220, 220));
    dark.setColor(QPalette::Button,          QColor(35, 35, 35));
    dark.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    dark.setColor(QPalette::Highlight,       QColor(0, 120, 215));
    dark.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(dark);

    MainWindow window;
    window.show();

    // ── Start CDP probe after a short delay to let a page load ──────────────
    auto *probe = new CdpProbe(&app);
    QTimer::singleShot(3000, [probe]() {
        probe->start();   // defaults to 127.0.0.1:9222
    });

    return app.exec();
}