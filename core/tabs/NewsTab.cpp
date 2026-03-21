#include "NewsTab.h"
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QMessageBox>

// ═════════════════════════════════════════════════════════════════════════════
//  Shared style
// ═════════════════════════════════════════════════════════════════════════════
QString NewsTab::s() {
    return R"(
        QWidget   { background:#0d0d0d; color:#cccccc; }
        QLabel    { background:transparent; }
        QScrollArea { border:none; background:#0d0d0d; }
        QScrollBar:vertical { background:#090909; width:5px; border:none; }
        QScrollBar::handle:vertical { background:#1e1e1e; border-radius:2px; min-height:16px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )";
}

QPushButton *NewsTab::actionBtn(const QString &label, const QString &color,
                                 QWidget *parent) {
    auto *b = new QPushButton(label, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(R"(
        QPushButton {
            background:#0d0d0d; color:%1;
            border:1px solid %1; border-radius:3px;
            font-family:monospace; font-size:11px; font-weight:bold;
            padding:6px 16px;
        }
        QPushButton:hover  { background:%2; }
        QPushButton:pressed{ background:%3; }
        QPushButton:disabled { color:#333; border-color:#222; }
    )").arg(color)
       .arg(color=="#00cc66"?"#001800":color=="#0088ff"?"#001020":
            color=="#ff4444"?"#1a0000":color=="#ffaa00"?"#1a1000":"#161616")
       .arg(color=="#00cc66"?"#002600":color=="#0088ff"?"#001830":
            color=="#ff4444"?"#2a0000":color=="#ffaa00"?"#2a1800":"#222"));
    return b;
}

QFrame *NewsTab::separator(QWidget *parent) {
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet("border:none; background:#1a1a1a; max-height:1px; margin:4px 0;");
    return f;
}

// ═════════════════════════════════════════════════════════════════════════════
//  NotificationBell
// ═════════════════════════════════════════════════════════════════════════════
NotificationBell::NotificationBell(QWidget *parent) : QPushButton(parent) {
    setFixedSize(32, 32);
    setCursor(Qt::PointingHandCursor);
    setText("🔔");
    m_badge = new QLabel("0", this);
    m_badge->setFixedSize(14, 14);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->move(18, 0);
    m_badge->hide();
    refreshStyle();
}

void NotificationBell::setUnread(int count) {
    m_unread = count;
    if (count > 0) {
        m_badge->setText(count > 9 ? "9+" : QString::number(count));
        m_badge->show();
    } else {
        m_badge->hide();
    }
    refreshStyle();
}

void NotificationBell::clearUnread() { setUnread(0); }

void NotificationBell::refreshStyle() {
    bool active = m_unread > 0;
    setStyleSheet(QString(R"(
        QPushButton {
            background:transparent; border:none;
            font-size:16px; color:%1;
        }
        QPushButton:hover { background:#1a1a1a; border-radius:4px; }
    )").arg(active ? "#ffaa00" : "#444444"));
    m_badge->setStyleSheet(R"(
        QLabel {
            background:#ff4444; color:white;
            border-radius:7px;
            font-family:monospace; font-size:8px; font-weight:bold;
        }
    )");
}

// ═════════════════════════════════════════════════════════════════════════════
//  NewsTab
// ═════════════════════════════════════════════════════════════════════════════
NewsTab::NewsTab(QWidget *parent) : QWidget(parent) {
    setStyleSheet(s());
    m_bell = new NotificationBell(this);
    buildUI();
}

void NewsTab::attachChecker(UpdateChecker *checker) {
    m_checker = checker;

    // Version check signals
    connect(checker, &UpdateChecker::updateAvailable, this, &NewsTab::onUpdateAvailable);
    connect(checker, &UpdateChecker::noUpdate,        this, &NewsTab::onNoUpdate);
    connect(checker, &UpdateChecker::checkFailed,     this, &NewsTab::onCheckFailed);

    // Download signals
    connect(checker, &UpdateChecker::downloadProgress, this, [this](int pct) {
        setDownloadProgress(pct);
    });
    connect(checker, &UpdateChecker::downloadReady, this, [this](const QString &path, const VersionInfo &info) {
        onDownloadReady(path, info);
    });
    connect(checker, &UpdateChecker::downloadFailed, this, [this](const QString &err) {
        onDownloadFailed(err);
    });
    connect(checker, &UpdateChecker::installStarted, this, [this]() {
        m_updateStatus->setStyleSheet(
            "color:#00cc66; font-family:monospace; font-size:11px; background:transparent;");
        m_updateStatus->setText("✓  Installing update... app will restart automatically.");
        m_downloadBtn->setEnabled(false);
        m_installBtn->setEnabled(false);
        m_cancelBtn->hide();
    });
}

void NewsTab::buildUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    auto *topBar = new QWidget(this);
    topBar->setFixedHeight(42);
    topBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *tbl = new QHBoxLayout(topBar);
    tbl->setContentsMargins(16,4,16,4); tbl->setSpacing(10);

    auto *title = new QLabel("NOTHING BROWSER", topBar);
    title->setStyleSheet(
        "color:#00cc66; font-family:monospace; font-size:13px; font-weight:bold; letter-spacing:2px;");

    m_versionLabel = new QLabel(
        QString("v%1").arg(UpdateChecker::CURRENT_VERSION), topBar);
    m_versionLabel->setStyleSheet("color:#333; font-family:monospace; font-size:11px;");

    tbl->addWidget(title);
    tbl->addWidget(m_versionLabel);
    tbl->addStretch();
    m_bell->setParent(topBar);
    tbl->addWidget(m_bell);
    root->addWidget(topBar);

    // ── Scrollable body ───────────────────────────────────────────────────────
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(s() + "QScrollArea { border:none; }");

    auto *body = new QWidget;
    body->setStyleSheet(s());
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(32, 24, 32, 32);
    bl->setSpacing(24);

    // ── Quick Access panel ────────────────────────────────────────────────────
    auto *qaCard = new QWidget(body);
    qaCard->setStyleSheet(
        "QWidget { background:#111111; border:1px solid #1e1e1e; border-radius:4px; }");
    auto *qal = new QVBoxLayout(qaCard);
    qal->setContentsMargins(20, 16, 20, 16);
    qal->setSpacing(12);

    auto *qaTitle = new QLabel("⚡  QUICK ACCESS", qaCard);
    qaTitle->setStyleSheet(
        "color:#00cc66; font-family:monospace; font-size:12px; font-weight:bold; "
        "letter-spacing:1px; background:transparent;");
    auto *qaDesc = new QLabel("Jump straight to the tools you need.", qaCard);
    qaDesc->setStyleSheet("color:#444; font-family:monospace; font-size:11px; background:transparent;");

    auto *btnRow = new QWidget(qaCard);
    btnRow->setStyleSheet("background:transparent;");
    auto *brl = new QHBoxLayout(btnRow);
    brl->setContentsMargins(0,0,0,0); brl->setSpacing(10);

    auto *scrBtn = actionBtn("⬡  OPEN DEVTOOLS / SCRAPPER", "#00cc66", btnRow);
    auto *brBtn  = actionBtn("◎  OPEN BROWSER",             "#0088ff", btnRow);
    connect(scrBtn, &QPushButton::clicked, this, &NewsTab::openScrapper);
    connect(brBtn,  &QPushButton::clicked, this, &NewsTab::openBrowser);
    brl->addWidget(scrBtn); brl->addWidget(brBtn); brl->addStretch();

    auto *statsRow = new QWidget(qaCard);
    statsRow->setStyleSheet("background:transparent;");
    auto *srl = new QHBoxLayout(statsRow);
    srl->setContentsMargins(0,0,0,0); srl->setSpacing(20);

    auto makeStat = [&](const QString &label, const QString &value) {
        auto *w = new QWidget(statsRow);
        w->setStyleSheet("background:transparent;");
        auto *wl = new QVBoxLayout(w);
        wl->setContentsMargins(0,0,0,0); wl->setSpacing(2);
        auto *lbl = new QLabel(label, w);
        lbl->setStyleSheet("color:#333; font-family:monospace; font-size:9px; background:transparent;");
        auto *val = new QLabel(value, w);
        val->setStyleSheet("color:#00cc66; font-family:monospace; font-size:13px; "
                           "font-weight:bold; background:transparent;");
        wl->addWidget(lbl); wl->addWidget(val);
        return w;
    };

    srl->addWidget(makeStat("CURRENT VERSION", UpdateChecker::CURRENT_VERSION));
    srl->addWidget(makeStat("ENGINE",          "Chromium/Qt6"));
    srl->addWidget(makeStat("PLATFORM",        "Linux"));
    srl->addStretch();

    qal->addWidget(qaTitle);
    qal->addWidget(qaDesc);
    qal->addWidget(separator(qaCard));
    qal->addWidget(btnRow);
    qal->addWidget(statsRow);
    bl->addWidget(qaCard);

    // ── Update card ───────────────────────────────────────────────────────────
    auto *updCard = new QWidget(body);
    updCard->setStyleSheet(
        "QWidget { background:#111111; border:1px solid #1e1e1e; border-radius:4px; }");
    auto *udl = new QVBoxLayout(updCard);
    udl->setContentsMargins(20, 16, 20, 16);
    udl->setSpacing(10);

    auto *udTitle = new QLabel("🔔  UPDATES", updCard);
    udTitle->setStyleSheet(
        "color:#ffaa00; font-family:monospace; font-size:12px; font-weight:bold; "
        "letter-spacing:1px; background:transparent;");

    m_updateStatus = new QLabel("Checking for updates...", updCard);
    m_updateStatus->setWordWrap(true);
    m_updateStatus->setStyleSheet(
        "color:#444; font-family:monospace; font-size:11px; background:transparent;");

    // Progress bar (hidden until download starts)
    m_progressWrap = new QWidget(updCard);
    m_progressWrap->hide();
    m_progressWrap->setStyleSheet("background:transparent;");
    auto *pwl = new QHBoxLayout(m_progressWrap);
    pwl->setContentsMargins(0,0,0,0); pwl->setSpacing(10);

    auto *progOuter = new QWidget(m_progressWrap);
    progOuter->setFixedHeight(4);
    progOuter->setMinimumWidth(200);
    progOuter->setStyleSheet("background:#1a1a00; border-radius:2px;");
    auto *progInner = new QHBoxLayout(progOuter);
    progInner->setContentsMargins(0,0,0,0); progInner->setSpacing(0);
    m_progressFill = new QWidget(progOuter);
    m_progressFill->setFixedWidth(0);
    m_progressFill->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #997700, stop:1 #ffaa00); border-radius:2px;");
    progInner->addWidget(m_progressFill);
    progInner->addStretch();

    m_progressLabel = new QLabel("0%", m_progressWrap);
    m_progressLabel->setStyleSheet(
        "color:#ffaa00; font-family:monospace; font-size:10px; background:transparent;");

    pwl->addWidget(progOuter, 1);
    pwl->addWidget(m_progressLabel);

    // Button row
    auto *btnRow2 = new QWidget(updCard);
    btnRow2->setStyleSheet("background:transparent;");
    auto *br2l = new QHBoxLayout(btnRow2);
    br2l->setContentsMargins(0,0,0,0); br2l->setSpacing(10);

    m_checkBtn    = actionBtn("↺  CHECK NOW",          "#888888", btnRow2);
    m_downloadBtn = actionBtn("↓  DOWNLOAD UPDATE",    "#ffaa00", btnRow2);
    m_installBtn  = actionBtn("⚡  INSTALL & RESTART", "#00cc66", btnRow2);
    m_cancelBtn   = actionBtn("✕  CANCEL",             "#ff4444", btnRow2);

    m_downloadBtn->hide();
    m_installBtn->hide();
    m_cancelBtn->hide();

    connect(m_checkBtn, &QPushButton::clicked, this, &NewsTab::onManualCheck);

    connect(m_downloadBtn, &QPushButton::clicked, this, [this]() {
        if (!m_checker || m_pendingInfo.downloadUrl.isEmpty()) return;
        m_downloadBtn->setEnabled(false);
        m_cancelBtn->show();
        m_progressWrap->show();
        m_updateStatus->setStyleSheet(
            "color:#ffaa00; font-family:monospace; font-size:11px; background:transparent;");
        m_updateStatus->setText(
            QString("Downloading v%1...").arg(m_pendingInfo.version));
        m_checker->startDownload(m_pendingInfo);
    });

    connect(m_installBtn, &QPushButton::clicked, this, [this]() {
        if (m_downloadedPath.isEmpty()) return;
        auto res = QMessageBox::question(this,
            "Install Update",
            QString("Install Nothing Browser v%1?\n\n"
                    "The app will close and restart automatically.")
                .arg(m_pendingInfo.version),
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes)
            m_checker->installUpdate(m_downloadedPath);
    });

    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        if (m_checker) m_checker->cancelDownload();
        m_progressWrap->hide();
        m_cancelBtn->hide();
        m_downloadBtn->setEnabled(true);
        m_installBtn->hide();
        m_progressFill->setFixedWidth(0);
        m_updateStatus->setText(
            QString("🔔 Update available: v%1  —  download cancelled")
                .arg(m_pendingInfo.version));
    });

    br2l->addWidget(m_checkBtn);
    br2l->addWidget(m_downloadBtn);
    br2l->addWidget(m_installBtn);
    br2l->addWidget(m_cancelBtn);
    br2l->addStretch();

    udl->addWidget(udTitle);
    udl->addWidget(m_updateStatus);
    udl->addWidget(m_progressWrap);
    udl->addWidget(separator(updCard));
    udl->addWidget(btnRow2);
    bl->addWidget(updCard);

    // ── Changelog card ────────────────────────────────────────────────────────
    auto *clCard = new QWidget(body);
    clCard->setStyleSheet(
        "QWidget { background:#111111; border:1px solid #1e1e1e; border-radius:4px; }");
    auto *cll = new QVBoxLayout(clCard);
    cll->setContentsMargins(20, 16, 20, 16);
    cll->setSpacing(8);

    auto *clTitle = new QLabel("📋  CHANGELOG", clCard);
    clTitle->setStyleSheet(
        "color:#0088ff; font-family:monospace; font-size:12px; font-weight:bold; "
        "letter-spacing:1px; background:transparent;");

    m_changelogWidget = new QWidget(clCard);
    m_changelogWidget->setStyleSheet("background:transparent;");
    m_changelogLayout = new QVBoxLayout(m_changelogWidget);
    m_changelogLayout->setContentsMargins(0,0,0,0);
    m_changelogLayout->setSpacing(4);

    renderChangelog({
        {"fix",    "Copy buttons now work across all panels"},
        {"fix",    "FingerprintSpoofer missing QStringList include"},
        {"fix",    "NetworkCapture missing QTimer include"},
        {"added",  "Full DevTools panel: Network / WS / Cookies / Storage / Export"},
        {"added",  "WebSocket frame capture + download"},
        {"added",  "Cookie request inspector (Set-By Request tab)"},
        {"added",  "Firefox-style request summary + raw HTTP view"},
        {"added",  "Welcome screen with scroll-to-accept"},
        {"added",  "Download button on every capture panel"},
        {"coming", "v0.2 — Windows support + response body search"},
        {"coming", "v0.3 — Built-in captcha solver"},
        {"coming", "v0.4 — Script marketplace + headless mode"},
    });

    cll->addWidget(clTitle);
    cll->addWidget(separator(clCard));
    cll->addWidget(m_changelogWidget);
    bl->addWidget(clCard);
    bl->addStretch();

    scroll->setWidget(body);
    root->addWidget(scroll, 1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Changelog renderer
// ═════════════════════════════════════════════════════════════════════════════
void NewsTab::renderChangelog(const QList<ChangeEntry> &entries) {
    QLayoutItem *item;
    while ((item = m_changelogLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    for (auto &e : entries) {
        QString color = e.type == "added"  ? "#00cc66" :
                        e.type == "fix"    ? "#0088ff" :
                        e.type == "coming" ? "#555555" : "#888888";
        QString tag   = e.type == "added"  ? "[+]" :
                        e.type == "fix"    ? "[F]" :
                        e.type == "coming" ? "[→]" : "[*]";
        auto *row = new QLabel(
            QString("<span style='color:%1; font-weight:bold;'>%2</span>"
                    "<span style='color:#555;'>  %3</span>")
                .arg(color).arg(tag).arg(e.text.toHtmlEscaped()),
            m_changelogWidget);
        row->setStyleSheet("font-family:monospace; font-size:11px; background:transparent;");
        m_changelogLayout->addWidget(row);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Progress bar helper
// ═════════════════════════════════════════════════════════════════════════════
void NewsTab::setDownloadProgress(int pct) {
    m_progressLabel->setText(QString("%1%").arg(pct));
    int outerW = m_progressWrap->width() - m_progressLabel->width() - 16;
    if (outerW > 0)
        m_progressFill->setFixedWidth(outerW * pct / 100);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Update checker callbacks
// ═════════════════════════════════════════════════════════════════════════════
void NewsTab::onUpdateAvailable(const VersionInfo &info) {
    m_pendingInfo = info;

    m_updateStatus->setStyleSheet(
        "color:#ffaa00; font-family:monospace; font-size:11px; background:transparent;");
    m_updateStatus->setText(
        QString("🔔  Update available: v%1  (you have v%2)  —  click DOWNLOAD UPDATE to get it")
            .arg(info.version).arg(UpdateChecker::CURRENT_VERSION));

    m_downloadBtn->show();
    m_installBtn->hide();
    m_bell->setUnread(1);

    if (!info.changelog.isEmpty())
        renderChangelog(info.changelog);
}

void NewsTab::onNoUpdate(const VersionInfo &info) {
    m_updateStatus->setStyleSheet(
        "color:#00cc66; font-family:monospace; font-size:11px; background:transparent;");
    m_updateStatus->setText(
        QString("✓  You are on the latest version (v%1)  —  checked %2")
            .arg(UpdateChecker::CURRENT_VERSION)
            .arg(QDateTime::currentDateTime().toString("hh:mm dd MMM")));
    m_downloadBtn->hide();
    m_installBtn->hide();
    m_bell->clearUnread();

    if (!info.changelog.isEmpty())
        renderChangelog(info.changelog);
}

void NewsTab::onCheckFailed(const QString &err) {
    m_updateStatus->setStyleSheet(
        "color:#555; font-family:monospace; font-size:11px; background:transparent;");
    m_updateStatus->setText("⚠  Could not reach update server  —  " + err);
}

void NewsTab::onManualCheck() {
    m_updateStatus->setStyleSheet(
        "color:#444; font-family:monospace; font-size:11px; background:transparent;");
    m_updateStatus->setText("Checking...");
    m_downloadBtn->hide();
    m_installBtn->hide();
    m_progressWrap->hide();
    if (m_checker) m_checker->checkNow();
}

void NewsTab::onDownloadReady(const QString &path, const VersionInfo &info) {
    m_downloadedPath = path;
    m_pendingInfo    = info;

    m_cancelBtn->hide();
    m_progressWrap->hide();
    m_installBtn->show();
    m_downloadBtn->hide();

    m_updateStatus->setStyleSheet(
        "color:#00cc66; font-family:monospace; font-size:11px; background:transparent;");
    m_updateStatus->setText(
        QString("✓  v%1 downloaded  —  click INSTALL & RESTART to apply the update")
            .arg(info.version));

    m_bell->setUnread(1);
}

void NewsTab::onDownloadFailed(const QString &err) {
    m_cancelBtn->hide();
    m_progressWrap->hide();
    m_downloadBtn->setEnabled(true);

    m_updateStatus->setStyleSheet(
        "color:#ff4444; font-family:monospace; font-size:11px; background:transparent;");
    m_updateStatus->setText("✕  Download failed  —  " + err);
}

void NewsTab::setUpdateBanner(bool, const QString &, const QString &) {}