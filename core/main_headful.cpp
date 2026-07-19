#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QVBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QMap>
#include "../engine/piggy/PiggyServer.h"
#include "engine/FingerprintSpoofer.h"
#include "engine/NetworkCapture.h"

class HeadfulWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit HeadfulWindow(PiggyServer *server, QWebEnginePage *defaultPage,
                           QWidget *parent = nullptr)
        : QMainWindow(parent), m_server(server)
    {
        setWindowTitle("Nothing Browser");
        resize(1280, 800);
        setMinimumSize(800, 600);
        setStyleSheet("QMainWindow { background:#121212; }");

        auto *central = new QWidget(this);
        auto *layout  = new QVBoxLayout(central);
        layout->setContentsMargins(0,0,0,0);
        layout->setSpacing(0);

        m_tabs = new QTabWidget(central);
        m_tabs->setTabsClosable(true);
        m_tabs->setMovable(true);
        m_tabs->setStyleSheet(R"(
            QTabWidget::pane { border:none; background:#121212; }
            QTabBar           { background:#0d0d0d; border-bottom:1px solid #1e1e1e; }
            QTabBar::tab {
                background:#111111; color:#555555;
                padding:6px 18px; border:none;
                border-right:1px solid #1a1a1a;
                font-family:monospace; font-size:11px; letter-spacing:1px;
            }
            QTabBar::tab:selected {
                background:#121212; color:#ffffff;
                border-bottom:2px solid #ff6688;
            }
            QTabBar::tab:hover { background:#181818; color:#aaaaaa; }
            QTabBar::tab:first { border-left:1px solid #1a1a1a; }
            QTabBar::close-button {
                image: none;
                subcontrol-position: right;
            }
        )");

        // + new tab button
        auto *newTabBtn = new QPushButton("+", m_tabs);
        newTabBtn->setFixedSize(28, 24);
        newTabBtn->setCursor(Qt::PointingHandCursor);
        newTabBtn->setStyleSheet(R"(
            QPushButton {
                background:#111; color:#555; border:none;
                font-size:16px; font-family:monospace; border-radius:3px;
            }
            QPushButton:hover { color:#ff6688; background:#1a1a1a; }
        )");
        m_tabs->setCornerWidget(newTabBtn, Qt::TopRightCorner);

        // wire default page as tab 0
        addPageAsTab(defaultPage, "New Tab");

        connect(m_tabs,   &QTabWidget::tabCloseRequested,
                this,     &HeadfulWindow::onTabCloseRequested);
        connect(newTabBtn, &QPushButton::clicked,
                this,      &HeadfulWindow::onNewTabClicked);

        // script-driven tab creation / deletion
        connect(server, &PiggyServer::tabCreated,
                this,   &HeadfulWindow::onScriptTabCreated);
        connect(server, &PiggyServer::tabClosed,
                this,   &HeadfulWindow::onScriptTabClosed);

        layout->addWidget(m_tabs);
        setCentralWidget(central);
    }

private slots:
    void onTabCloseRequested(int idx) {
        if (m_tabs->count() <= 1) return;   // never close last tab
        auto *view = qobject_cast<QWebEngineView*>(m_tabs->widget(idx));
        if (!view) return;
        QWebEnginePage *pg = view->page();
        // remove from maps
        m_pageToTabIndex.remove(pg);
        QString tabId = m_pageToTabId.value(pg);
        if (!tabId.isEmpty()) m_tabIdToPage.remove(tabId);
        m_pageToTabId.remove(pg);
        m_tabs->removeTab(idx);
        rebuildIndexMap();
    }

    void onNewTabClicked() {
        // user-opened blank tab — not script-managed
        auto *profile = new QWebEngineProfile(this);
        profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
        auto *pg = new QWebEnginePage(profile, this);
        pg->load(QUrl("about:blank"));
        addPageAsTab(pg, "New Tab");
    }

    void onScriptTabCreated(const QString &tabId, QWebEnginePage *pg) {
        addPageAsTab(pg, "tab:" + tabId.left(8));
        m_tabIdToPage[tabId] = pg;
        m_pageToTabId[pg]    = tabId;
    }

    void onScriptTabClosed(const QString &tabId) {
        QWebEnginePage *pg = m_tabIdToPage.value(tabId, nullptr);
        if (!pg) return;
        int idx = m_pageToTabIndex.value(pg, -1);
        if (idx >= 0 && m_tabs->count() > 1)
            m_tabs->removeTab(idx);
        m_tabIdToPage.remove(tabId);
        m_pageToTabId.remove(pg);
        m_pageToTabIndex.remove(pg);
        rebuildIndexMap();
    }

private:
    void addPageAsTab(QWebEnginePage *pg, const QString &title) {
        auto *view = new QWebEngineView(this);
        view->setPage(pg);

        int idx = m_tabs->addTab(view, title);
        m_tabs->setCurrentIndex(idx);
        m_pageToTabIndex[pg] = idx;

        // update tab title when page loads
        connect(pg, &QWebEnginePage::titleChanged,
                this, [this, pg](const QString &t) {
            int i = m_pageToTabIndex.value(pg, -1);
            if (i >= 0)
                m_tabs->setTabText(i, t.isEmpty() ? "New Tab" : t.left(28));
        });

        // update tab title on url change for blank tabs
        connect(pg, &QWebEnginePage::urlChanged,
                this, [this, pg](const QUrl &url) {
            int i = m_pageToTabIndex.value(pg, -1);
            if (i >= 0 && m_tabs->tabText(i) == "New Tab" && !url.isEmpty())
                m_tabs->setTabText(i, url.host().left(28));
        });
    }

    void rebuildIndexMap() {
        m_pageToTabIndex.clear();
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *view = qobject_cast<QWebEngineView*>(m_tabs->widget(i));
            if (view) m_pageToTabIndex[view->page()] = i;
        }
    }

    QTabWidget                      *m_tabs;
    PiggyServer                     *m_server;
    QMap<QWebEnginePage*, int>       m_pageToTabIndex;
    QMap<QWebEnginePage*, QString>   m_pageToTabId;
    QMap<QString, QWebEnginePage*>   m_tabIdToPage;
};

// ─── main ────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    // FIX: added --disable-dev-shm-usage       → prevents shm SIGSEGV on low-RAM machines
    //      added --disable-site-isolation-trials → disables COOP renderer isolation
    //        that crashes on ChatGPT (and other sites with COOP/COEP headers)
    //      added --disable-features=IsolateOrigins → same family
    //      added --js-flags                     → cap renderer heap for i3/8GB
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
    app.setApplicationName("nothing-browser-headful");
    app.setApplicationVersion("0.1.0");

    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(18,18,18));
    dark.setColor(QPalette::WindowText,      QColor(220,220,220));
    dark.setColor(QPalette::Base,            QColor(24,24,24));
    dark.setColor(QPalette::AlternateBase,   QColor(30,30,30));
    dark.setColor(QPalette::Text,            QColor(220,220,220));
    dark.setColor(QPalette::Button,          QColor(35,35,35));
    dark.setColor(QPalette::ButtonText,      QColor(220,220,220));
    dark.setColor(QPalette::Highlight,       QColor(255,102,136));
    dark.setColor(QPalette::HighlightedText, QColor(255,255,255));
    app.setPalette(dark);

    // ── shared profile ────────────────────────────────────────────────────────
    auto *profile = new QWebEngineProfile(&app);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    auto &spoofer = FingerprintSpoofer::instance();
    profile->setHttpUserAgent(spoofer.identity().userAgent);

    // FIX: DocumentCreation instead of DocumentReady.
    //      DocumentReady on COOP-isolated pages (ChatGPT) races the renderer
    //      context setup → SIGSEGV exit 139. DocumentCreation fires before
    //      any page JS runs — stable on all sites including COOP ones.
    QWebEngineScript spoofScript;
    spoofScript.setName("nothing_fingerprint");
    spoofScript.setSourceCode(spoofer.injectionScript());
    spoofScript.setInjectionPoint(QWebEngineScript::DocumentCreation); // FIX
    spoofScript.setWorldId(QWebEngineScript::MainWorld);
    spoofScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(spoofScript);

    QWebEngineScript capScript;
    capScript.setName("nothing_capture");
    capScript.setSourceCode(NetworkCapture::captureScript());
    capScript.setInjectionPoint(QWebEngineScript::DocumentCreation); // FIX
    capScript.setWorldId(QWebEngineScript::MainWorld);
    capScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(capScript);

    // ── default page ──────────────────────────────────────────────────────────
    auto *defaultPage = new QWebEnginePage(profile, &app);
    defaultPage->setHtml(R"HTML(
<!DOCTYPE html><html>
<head><meta charset="utf-8">
<style>
  *{margin:0;padding:0;box-sizing:border-box;}
  body{background:#0a0a0a;display:flex;flex-direction:column;
       align-items:center;justify-content:center;height:100vh;
       font-family:monospace;color:#333;user-select:none;}
  .pig{font-size:56px;margin-bottom:18px;opacity:0.5;}
  .head{font-size:13px;color:#ff6688;letter-spacing:3px;margin-bottom:10px;}
  .sub{font-size:11px;color:#2a2a2a;letter-spacing:1px;}
</style>
</head>
<body>
  <div class="pig">🐷</div>
  <div class="head">NOTHING BROWSER</div>
  <div class="sub">waiting for script...</div>
</body>
</html>
)HTML");

    // ── server + window ───────────────────────────────────────────────────────
    auto *server = new PiggyServer(defaultPage, &app);
    server->start();

    HeadfulWindow window(server, defaultPage);
    window.show();

    qDebug() << "[HeadfulPiggy] Window open, socket: piggy";
    return app.exec();
}

#include "main_headful.moc"