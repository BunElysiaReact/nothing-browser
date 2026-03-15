#include "MainWindow.h"
#include "../tabs/BrowserTab.h"
#include "../tabs/YoutubeTab.h"
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Nothing Browser");
    resize(1400, 900);
    setMinimumSize(900, 600);
    setupStyle();

    // ── Update checker (starts polling after 3s) ──────────────────────────────
    m_checker = new UpdateChecker(this);

    // ── Stack: welcome (0) → main UI (1) ─────────────────────────────────────
    m_stack   = new QStackedWidget(this);
    m_welcome = new WelcomeScreen(m_stack);
    m_main    = new QWidget(m_stack);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_main);
    m_stack->setCurrentIndex(0);
    setCentralWidget(m_stack);

    connect(m_welcome, &WelcomeScreen::accepted, this, [this]() {
        m_stack->setCurrentIndex(1);
        statusBar()->showMessage("Nothing Browser v0.1  —  META SCRAPPER READY");
    });

    setupTabs();
}

void MainWindow::setupTabs() {
    auto *root = new QVBoxLayout(m_main);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    m_tabs = new QTabWidget(m_main);
    m_tabs->setMovable(false);
    m_tabs->setTabsClosable(false);

    m_devtools = new DevToolsPanel(m_main);
    m_browser  = new BrowserTab(m_main);
    m_youtube  = new YoutubeTab(m_main);
    m_news     = new NewsTab(m_main);

    // Wire update checker into news tab
    m_news->attachChecker(m_checker);

    // Flash tab label when update arrives
    connect(m_checker, &UpdateChecker::updateAvailable, this,
            [this](const VersionInfo &info) {
        m_tabs->setTabText(3,
            QString("🔔 TECH HOUSE  [v%1 ready]").arg(info.version));
    });

    // Quick-access buttons in NewsTab switch tabs
    connect(m_news, &NewsTab::openScrapper, this, [this](){
        m_tabs->setCurrentIndex(0); // DevTools is tab 0
    });
    connect(m_news, &NewsTab::openBrowser, this, [this](){
        m_tabs->setCurrentIndex(1); // Browser is tab 1
    });

    m_tabs->addTab(m_devtools, tabIcon("#00cc66"), "DEVTOOLS");
    m_tabs->addTab(m_browser,  tabIcon("#0088ff"), "BROWSER");
    m_tabs->addTab(m_youtube,  tabIcon("#ff4444"), "YOUTUBE");
    m_tabs->addTab(m_news,     tabIcon("#ffaa00"), "TECH HOUSE");

    // ── Wire NetworkCapture → DevToolsPanel ──────────────────────────────────
    auto *cap = m_browser->networkCapture();
    connect(cap, &NetworkCapture::requestCaptured,
            m_devtools, &DevToolsPanel::onRequestCaptured);
    connect(cap, &NetworkCapture::wsFrameCaptured,
            m_devtools, &DevToolsPanel::onWsFrame);
    connect(cap, &NetworkCapture::cookieCaptured,
            m_devtools, &DevToolsPanel::onCookieCaptured);
    connect(cap, &NetworkCapture::cookieRemoved,
            m_devtools, &DevToolsPanel::onCookieRemoved);
    connect(cap, &NetworkCapture::storageCaptured,
            m_devtools, &DevToolsPanel::onStorageCaptured);
    connect(m_browser, &BrowserTab::requestCaptured,
            m_devtools, &DevToolsPanel::onRawRequest);

    root->addWidget(m_tabs);
}

QIcon MainWindow::tabIcon(const QString &color) {
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(color));
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 12, 12);
    p.end();
    return QIcon(pm);
}

void MainWindow::setupStyle() {
    setStyleSheet(R"(
        QMainWindow { background:#121212; }
        QTabWidget::pane { border:none; background:#121212; }
        QTabBar { background:#0d0d0d; border-bottom:1px solid #1e1e1e; }
        QTabBar::tab {
            background:#111111; color:#555555;
            padding:8px 20px; border:none;
            border-right:1px solid #1a1a1a;
            font-size:11px; font-family:monospace; letter-spacing:1px;
        }
        QTabBar::tab:selected {
            background:#121212; color:#ffffff;
            border-bottom:2px solid #0078d7;
        }
        QTabBar::tab:hover { background:#181818; color:#aaaaaa; }
        QTabBar::tab:first { border-left:1px solid #1a1a1a; }
        QStatusBar {
            background:#0a0a0a; color:#00cc66;
            font-family:monospace; font-size:11px;
            border-top:1px solid #1a1a1a;
        }
    )");
}