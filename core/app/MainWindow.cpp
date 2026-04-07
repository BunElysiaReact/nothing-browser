#include "MainWindow.h"
#include "../tabs/BrowserTab.h"
#include "../tabs/YoutubeTab.h"
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Nothing Browser");
    resize(1400, 900);
    setMinimumSize(900, 600);
    setupStyle();

    m_checker = new UpdateChecker(this);

    m_stack   = new QStackedWidget(this);
    m_welcome = new WelcomeScreen(m_stack);
    m_main    = new QWidget(m_stack);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_main);
    m_stack->setCurrentIndex(0);
    setCentralWidget(m_stack);

    connect(m_welcome, &WelcomeScreen::accepted, this, [this]() {
        m_stack->setCurrentIndex(1);
        statusBar()->showMessage(
            "Nothing Browser v0.1  —  META SCRAPPER READY  |  "
            "Sessions auto-saved on close");
    });

    setupTabs();
    setupSessionMenu();

    QDir().mkpath(sessionsDir());
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QString autoSavePath = sessionsDir() + "/last-session.json";
    bool saved = m_devtools->exportSession(autoSavePath);
    if (saved)
        statusBar()->showMessage("Session saved → " + autoSavePath);
    event->accept();
}

QString MainWindow::sessionsDir() const {
    return QStandardPaths::writableLocation(
               QStandardPaths::AppConfigLocation) + "/sessions";
}

void MainWindow::setupSessionMenu() {
    auto *menuBar = new QMenuBar(this);
    menuBar->setStyleSheet(R"(
        QMenuBar {
            background:#0d0d0d; color:#555;
            font-family:monospace; font-size:11px;
            border-bottom:1px solid #1a1a1a;
        }
        QMenuBar::item:selected { background:#1a1a1a; color:#ccc; }
        QMenu {
            background:#0d0d0d; color:#cccccc;
            border:1px solid #1a1a1a;
            font-family:monospace; font-size:11px;
        }
        QMenu::item:selected { background:#1a2a1a; }
        QMenu::separator { background:#1a1a1a; height:1px; }
    )");

    auto *sessionMenu = menuBar->addMenu("SESSION");

    auto *saveAct = new QAction("Save Current Session...", this);
    saveAct->setShortcut(QKeySequence("Ctrl+S"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveSession);

    auto *quickSaveAct = new QAction("Quick Save (auto-name)", this);
    quickSaveAct->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(quickSaveAct, &QAction::triggered, this, [this]() {
        quickSaveSession(QDateTime::currentDateTime()
                             .toString("yyyy-MM-dd_hh-mm-ss"));
    });

    auto *loadAct = new QAction("Load Session...", this);
    loadAct->setShortcut(QKeySequence("Ctrl+O"));
    connect(loadAct, &QAction::triggered, this, &MainWindow::loadSession);

    auto *loadLastAct = new QAction("Load Last Session", this);
    loadLastAct->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(loadLastAct, &QAction::triggered, this, [this]() {
        QString path = sessionsDir() + "/last-session.json";
        if (!QFile::exists(path)) {
            QMessageBox::information(this, "No Session",
                "No auto-saved session found.\n"
                "Browse normally first, then close — "
                "the session will be saved automatically.");
            return;
        }
        if (m_devtools->importSession(path))
            statusBar()->showMessage("Loaded last session ← " + path);
        else
            QMessageBox::warning(this, "Load Failed",
                "Could not load session from:\n" + path);
    });

    auto *openFolderAct = new QAction("Open Sessions Folder", this);
    connect(openFolderAct, &QAction::triggered, this, [this]() {
        QDir().mkpath(sessionsDir());
        QProcess::startDetached("xdg-open", { sessionsDir() });
    });

    sessionMenu->addAction(saveAct);
    sessionMenu->addAction(quickSaveAct);
    sessionMenu->addSeparator();
    sessionMenu->addAction(loadAct);
    sessionMenu->addAction(loadLastAct);
    sessionMenu->addSeparator();
    sessionMenu->addAction(openFolderAct);

    setMenuBar(menuBar);
}

void MainWindow::saveSession() {
    bool ok;
    QString name = QInputDialog::getText(
        this, "Save Session", "Session name:",
        QLineEdit::Normal,
        QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm"),
        &ok);
    if (!ok || name.isEmpty()) return;
    name.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "-");
    quickSaveSession(name);
}

void MainWindow::quickSaveSession(const QString &name) {
    QDir().mkpath(sessionsDir());
    QString path = sessionsDir() + "/" + name + ".json";
    if (m_devtools->exportSession(path)) {
        statusBar()->showMessage(QString("Session saved → %1").arg(path));
        QMessageBox::information(this, "Session Saved",
            QString("Saved to:\n%1\n\nLoad it anytime via Session → Load Session").arg(path));
    } else {
        QMessageBox::warning(this, "Save Failed",
            "Could not save session to:\n" + path);
    }
}

void MainWindow::loadSession() {
    QDir().mkpath(sessionsDir());
    QString path = QFileDialog::getOpenFileName(
        this, "Load Session", sessionsDir(),
        "Nothing Browser Sessions (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (m_devtools->importSession(path))
        statusBar()->showMessage("Session loaded ← " + path);
    else
        QMessageBox::warning(this, "Load Failed",
            "Could not load session from:\n" + path);
}

void MainWindow::setupTabs() {
    auto *root = new QVBoxLayout(m_main);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    m_tabs = new QTabWidget(m_main);
    m_tabs->setMovable(false);
    m_tabs->setTabsClosable(false);
    m_piggy = new PiggyTab(m_main);
    m_devtools = new DevToolsPanel(m_main);
    m_browser  = new BrowserTab(m_main);
    m_youtube  = new YoutubeTab(m_main);
    m_news     = new NewsTab(m_main);
    m_plugins  = new PluginsTab(m_main);
    

    m_news->attachChecker(m_checker);

    connect(m_checker, &UpdateChecker::updateAvailable, this,
            [this](const VersionInfo &info) {
        m_tabs->setTabText(3,
            QString("🔔 TECH HOUSE [v%1 ready]").arg(info.version));
    });

    connect(m_news, &NewsTab::openScrapper, this,
            [this](){ m_tabs->setCurrentIndex(0); });
    connect(m_news, &NewsTab::openBrowser,  this,
            [this](){ m_tabs->setCurrentIndex(1); });

    connect(m_browser, &BrowserTab::requestCaptured, this,
            [this](const QString&, const QString &url, const QString&) {
        if (!url.isEmpty()) m_devtools->setCurrentUrl(url);
    });

    m_tabs->addTab(m_devtools, tabIcon("#00cc66"), "DEVTOOLS");
    m_tabs->addTab(m_browser,  tabIcon("#0088ff"), "BROWSER");
    m_tabs->addTab(m_youtube,  tabIcon("#ff4444"), "YOUTUBE");
    m_tabs->addTab(m_news,     tabIcon("#ffaa00"), "TECH HOUSE");
    m_tabs->addTab(m_plugins,  tabIcon("#cc44ff"), "PLUGINS");
    m_tabs->addTab(m_piggy, tabIcon("#ff6688"), "PIGGY TAB");

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
    m_piggyServer = new PiggyServer(m_piggy, this);
    m_piggyServer->start();
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