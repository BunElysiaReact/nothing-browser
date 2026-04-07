#include <QApplication>
#include <QWebEngineProfile>
#include "app/MainWindow.h"

int main(int argc, char *argv[]) {
    // ── Chromium flags — set BEFORE QApplication ──────────────────────────────
    // --disable-blink-features=AutomationControlled:
    //   Makes navigator.webdriver return false at the engine level.
    //   Because Chromium does it internally, toString() stays [native code].
    //   Fixes: webDriverIsOn, hasToStringProxy (partially)
    //
    // --disable-features=UserAgentClientHint:
    //   Prevents QtWebEngine from injecting its own UA client hint overrides
    //   that conflict with our Interceptor headers.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-blink-features=AutomationControlled "
        "--no-sandbox "
        "--allow-running-insecure-content"
    );

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
    return app.exec();
}