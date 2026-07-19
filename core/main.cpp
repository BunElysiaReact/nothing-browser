#include <QApplication>
#include <QWebEngineProfile>
#include "app/MainWindow.h"

int main(int argc, char *argv[]) {
    // ── Chromium flags — set BEFORE QApplication ──────────────────────────────
    // --disable-blink-features=AutomationControlled:
    //   Makes navigator.webdriver return false at the engine level.
    //
    // --disable-dev-shm-usage:
    //   FIX: prevents SIGSEGV (exit 139) on low-RAM machines — Chromium renderer
    //   tries to use /dev/shm for shared memory; on machines with limited shm
    //   (or low RAM like i3/8GB) this causes the renderer to segfault.
    //   Forces renderer to use /tmp instead. Safe on all Linux setups.
    //
    // --disable-site-isolation-trials / --disable-features=IsolateOrigins:
    //   FIX: ChatGPT sends COOP/COEP headers which trigger Chromium's renderer
    //   process isolation. On QtWebEngine this races the JS context setup during
    //   script injection and causes exit 139. Disabling prevents the crash.
    //   Note: this does NOT disable HTTPS or origin security — just the extra
    //   process-level renderer sandboxing that QtWebEngine can't recover from.
    //
    // --js-flags=--max-old-space-size=512:
    //   FIX: caps V8 heap at 512 MB so the renderer doesn't OOM-kill on i3/8GB.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-blink-features=AutomationControlled "
        "--no-sandbox "
        "--allow-running-insecure-content "
        "--disable-dev-shm-usage "
        "--disable-site-isolation-trials "
        "--disable-features=IsolateOrigins,WebRtcHideLocalIpsWithMdns "
        "--js-flags=--max-old-space-size=512"
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