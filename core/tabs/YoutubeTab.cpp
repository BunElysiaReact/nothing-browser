#include "YoutubeTab.h"
#include <QSplitter>
#include <QJsonDocument>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QGraphicsOpacityEffect>

// ═════════════════════════════════════════════════════════════════════════════
//  VideoCard
// ═════════════════════════════════════════════════════════════════════════════
VideoCard::VideoCard(const YTResult &r, int idx,
                     QNetworkAccessManager *nam, QWidget *parent)
    : QWidget(parent), m_idx(idx)
{
    setFixedWidth(220);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet("VideoCard { background:#181818; border-radius:8px; }");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,8);
    root->setSpacing(4);

    // ── Thumbnail ─────────────────────────────────────────────────────────────
    auto *thumbWrap = new QWidget(this);
    thumbWrap->setFixedSize(220,124);
    thumbWrap->setStyleSheet("background:#0f0f0f; border-radius:8px 8px 0 0;");

    m_thumb = new QLabel(thumbWrap);
    m_thumb->setGeometry(0,0,220,124);
    m_thumb->setAlignment(Qt::AlignCenter);
    m_thumb->setStyleSheet(
        "background:#0f0f0f; color:#333; font-size:28px;"
        "border-radius:8px 8px 0 0;");
    m_thumb->setText("▶");

    m_duration = new QLabel(thumbWrap);
    m_duration->setAlignment(Qt::AlignCenter);
    m_duration->setStyleSheet(
        "background:rgba(0,0,0,0.85); color:#fff;"
        "font-family:'JetBrains Mono',monospace; font-size:10px;"
        "font-weight:bold; border-radius:3px; padding:1px 5px;");
    m_duration->setText(r.duration <= 0 ? "LIVE" :
        (r.duration >= 3600
            ? QString("%1:%2:%3").arg(r.duration/3600)
                .arg((r.duration%3600)/60,2,10,QChar('0'))
                .arg(r.duration%60,2,10,QChar('0'))
            : QString("%1:%2").arg(r.duration/60)
                .arg(r.duration%60,2,10,QChar('0'))));
    m_duration->adjustSize();
    m_duration->move(220-m_duration->width()-6, 124-m_duration->height()-6);

    // ── Text ─────────────────────────────────────────────────────────────────
    auto *textArea = new QWidget(this);
    textArea->setStyleSheet("background:transparent;");
    auto *tl = new QVBoxLayout(textArea);
    tl->setContentsMargins(10,2,10,0);
    tl->setSpacing(2);

    m_title = new QLabel(r.title, textArea);
    m_title->setWordWrap(true);
    m_title->setMaximumHeight(36);
    m_title->setStyleSheet(
        "color:#f1f1f1; font-size:12px; font-weight:bold;"
        "font-family:'Segoe UI','Noto Sans',sans-serif; background:transparent;");

    m_channel = new QLabel(r.uploader, textArea);
    m_channel->setStyleSheet(
        "color:#aaa; font-size:11px;"
        "font-family:'Segoe UI','Noto Sans',sans-serif; background:transparent;");

    qint64 v = r.views;
    QString viewStr;
    if      (v>=1'000'000'000) viewStr=QString::number(v/1'000'000'000.0,'f',1)+"B views";
    else if (v>=1'000'000)     viewStr=QString::number(v/1'000'000.0,'f',1)+"M views";
    else if (v>=1'000)         viewStr=QString::number(v/1'000.0,'f',0)+"K views";
    else                       viewStr=QString::number(v)+" views";

    m_views = new QLabel(viewStr, textArea);
    m_views->setStyleSheet(
        "color:#aaa; font-size:11px;"
        "font-family:'Segoe UI','Noto Sans',sans-serif; background:transparent;");

    // ── Watch Next button ─────────────────────────────────────────────────────
    auto *queueBtn = new QPushButton("+ Watch Next", textArea);
    queueBtn->setCursor(Qt::PointingHandCursor);
    queueBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent; color: #ff4444;
            border: 1px solid #ff4444; border-radius: 3px;
            font-size: 10px; padding: 3px 8px;
            font-family: 'Segoe UI', sans-serif;
        }
        QPushButton:hover { background: #ff4444; color: #fff; }
    )");
    connect(queueBtn, &QPushButton::clicked, this, [this]() {
        emit addToQueue(m_idx);
    });

    tl->addWidget(m_title);
    tl->addWidget(m_channel);
    tl->addWidget(m_views);
    tl->addWidget(queueBtn);

    root->addWidget(thumbWrap);
    root->addWidget(textArea);
    setFixedHeight(240);

    // ── Fetch thumbnail ───────────────────────────────────────────────────────
    if (!r.thumbnail.isEmpty()) {
        QNetworkRequest req(QUrl(r.thumbnail));
        req.setRawHeader("User-Agent",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        req.setRawHeader("Referer", "https://www.youtube.com/");
        QNetworkReply *reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            QByteArray data = reply->readAll();
            if (data.isEmpty()) return;
            QPixmap px;
            if (!px.loadFromData(data)) return;
            px = px.scaled(220,124,Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation);
            QPixmap cropped(220,124);
            cropped.fill(Qt::transparent);
            QPainter p(&cropped);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(0,0,220,124,8,8);
            p.setClipPath(path);
            p.drawPixmap((220-px.width())/2,(124-px.height())/2,px);
            p.end();
            m_thumb->setPixmap(cropped);
            m_thumb->setText("");
            m_thumb->setStyleSheet("background:transparent;");
        });
    }
}

void VideoCard::enterEvent(QEnterEvent *) {
    setStyleSheet("VideoCard { background:#222; border-radius:8px; }");
}
void VideoCard::leaveEvent(QEvent *) {
    setStyleSheet("VideoCard { background:#181818; border-radius:8px; }");
}

// ═════════════════════════════════════════════════════════════════════════════
//  MiniPlayer
// ═════════════════════════════════════════════════════════════════════════════
MiniPlayer::MiniPlayer(QWidget *parent) : QWidget(parent) {
    setFixedHeight(64);
    setStyleSheet(R"(
        MiniPlayer {
            background: rgba(20,20,20,0.95);
            border-top: 1px solid #ff4444;
            border-radius: 0;
        }
    )");

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(16,0,16,0);
    row->setSpacing(12);

    // ▲ red accent bar on left
    auto *accent = new QWidget(this);
    accent->setFixedSize(3,36);
    accent->setStyleSheet("background:#ff4444; border-radius:2px;");

    // Track info
    auto *infoCol = new QVBoxLayout;
    infoCol->setSpacing(2);
    m_titleLbl = new QLabel("Nothing playing", this);
    m_titleLbl->setStyleSheet(
        "color:#f1f1f1; font-size:12px; font-weight:bold;"
        "font-family:'Segoe UI',sans-serif; background:transparent;");
    m_channelLbl = new QLabel("", this);
    m_channelLbl->setStyleSheet(
        "color:#aaa; font-size:10px;"
        "font-family:'Segoe UI',sans-serif; background:transparent;");
    m_queueLbl = new QLabel("", this);
    m_queueLbl->setStyleSheet(
        "color:#555; font-size:10px; font-family:monospace; background:transparent;");
    infoCol->addWidget(m_titleLbl);
    infoCol->addWidget(m_channelLbl);
    infoCol->addWidget(m_queueLbl);

    // Controls
    auto mkCtrlBtn = [&](const QString &label) {
        auto *b = new QPushButton(label, this);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(32,32);
        b->setStyleSheet(R"(
            QPushButton {
                background:transparent; color:#aaa;
                border:1px solid #333; border-radius:16px;
                font-size:14px;
            }
            QPushButton:hover { color:#fff; border-color:#ff4444; }
        )");
        return b;
    };

    m_prevBtn   = mkCtrlBtn("⏮");
    m_nextBtn   = mkCtrlBtn("⏭");
    m_loopBtn   = mkCtrlBtn("⟳");
    m_expandBtn = mkCtrlBtn("⤢");
    m_expandBtn->setToolTip("Go to player");

    connect(m_prevBtn,   &QPushButton::clicked, this, &MiniPlayer::requestPrev);
    connect(m_nextBtn,   &QPushButton::clicked, this, &MiniPlayer::requestNext);
    connect(m_loopBtn,   &QPushButton::clicked, this, &MiniPlayer::requestToggleLoop);
    connect(m_expandBtn, &QPushButton::clicked, this, &MiniPlayer::requestExpand);

    row->addWidget(accent);
    row->addLayout(infoCol, 1);
    row->addWidget(m_prevBtn);
    row->addWidget(m_nextBtn);
    row->addWidget(m_loopBtn);
    row->addWidget(m_expandBtn);
}

void MiniPlayer::setNowPlaying(const QString &title, const QString &channel) {
    m_titleLbl->setText(title);
    m_channelLbl->setText(channel);
}

void MiniPlayer::setQueueSize(int size, int current) {
    if (size <= 0)
        m_queueLbl->setText("");
    else
        m_queueLbl->setText(QString("queue: %1 / %2").arg(current+1).arg(size));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Button factory
// ═════════════════════════════════════════════════════════════════════════════
QPushButton *YoutubeTab::makeBtn(const QString &label, const QString &fg,
                                  const QString &bg, QWidget *parent) {
    auto *b = new QPushButton(label, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(R"(
        QPushButton {
            background:%2; color:%1; border:1px solid %1;
            border-radius:4px;
            font-family:'Segoe UI',sans-serif;
            font-size:12px; font-weight:bold; padding:7px 16px;
        }
        QPushButton:hover   { background:%1; color:#0f0f0f; }
        QPushButton:pressed { opacity:0.8; }
        QPushButton:disabled { color:#333; border-color:#222; background:#111; }
    )").arg(fg,bg));
    return b;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Constructor
// ═════════════════════════════════════════════════════════════════════════════
YoutubeTab::YoutubeTab(QWidget *parent) : QWidget(parent) {
    m_nam = new QNetworkAccessManager(this);

    setStyleSheet(R"(
        QWidget      { background:#0f0f0f; color:#f1f1f1; }
        QScrollArea  { border:none; background:#0f0f0f; }
        QScrollBar:vertical { background:#0f0f0f; width:6px; border:none; }
        QScrollBar::handle:vertical { background:#333; border-radius:3px; min-height:20px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { height:0; }
        QLineEdit {
            background:#121212; color:#f1f1f1;
            border:1px solid #303030; border-radius:20px;
            padding:8px 18px; font-size:14px;
            font-family:'Segoe UI','Noto Sans',sans-serif;
        }
        QLineEdit:focus { border-color:#ff4444; }
        QComboBox {
            background:#1e1e1e; color:#f1f1f1;
            border:1px solid #303030; border-radius:4px;
            padding:6px 10px; font-size:12px;
        }
        QComboBox QAbstractItemView {
            background:#1e1e1e; color:#f1f1f1;
            selection-background-color:#2a0000;
            border:1px solid #303030;
        }
        QComboBox::drop-down { border:none; }
        QTextEdit {
            background:#111; color:#888; border:none;
            font-size:12px; padding:6px;
            font-family:'Segoe UI','Noto Sans',sans-serif;
        }
        QProgressBar {
            background:#222; border:none; border-radius:3px;
            height:6px;
        }
        QProgressBar::chunk { background:#ff4444; border-radius:3px; }
        QListWidget {
            background:#111; border:none;
            font-family:'Segoe UI',sans-serif; font-size:11px;
        }
        QListWidget::item { padding:6px 10px; border-bottom:1px solid #1a1a1a; color:#ccc; }
        QListWidget::item:selected { background:#2a0000; color:#ff4444; }
        QListWidget::item:hover { background:#1a1a1a; }
        QLabel { background:transparent; }
        QSplitter::handle { background:#1e1e1e; }
    )");

    m_pages = new QStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    root->addWidget(m_pages);

    // Auto-next timer — polls every 2s to check if video ended
    m_autoNextTimer = new QTimer(this);
    m_autoNextTimer->setInterval(2000);
    connect(m_autoNextTimer, &QTimer::timeout, this, &YoutubeTab::onCheckAutoNext);

    buildHomePage();
    buildResultsPage();
    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_resultsPage);
    m_pages->setCurrentIndex(0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Home Page
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::buildHomePage() {
    m_homePage = new QWidget;

    auto *root = new QVBoxLayout(m_homePage);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // Background container with Elysia
    auto *bgContainer = new QWidget(m_homePage);
    bgContainer->setObjectName("elysiaBackground");
    bgContainer->setStyleSheet(R"(
        QWidget#elysiaBackground {
            background-image: url(:/icons/elysia.jpeg);
            background-repeat: no-repeat;
            background-position: center;
            background-color: #0f0f0f;
        }
    )");
    bgContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Dark overlay
    auto *overlay = new QWidget(bgContainer);
    auto pal = overlay->palette();
    pal.setColor(QPalette::Window, QColor(0,0,0,165));
    overlay->setAutoFillBackground(true);
    overlay->setPalette(pal);

    auto *bgLayout = new QVBoxLayout(bgContainer);
    bgLayout->setContentsMargins(0,0,0,0);
    bgLayout->addWidget(overlay);

    auto *contentLayout = new QVBoxLayout(overlay);
    contentLayout->setContentsMargins(0,0,0,0);
    contentLayout->setSpacing(0);
    contentLayout->addStretch(2);

    // Logo
    auto *logoRow = new QHBoxLayout;
    logoRow->setAlignment(Qt::AlignHCenter);
    auto *logoLabel = new QLabel;
    logoLabel->setTextFormat(Qt::RichText);
    logoLabel->setText(
        "<span style='color:#ff4444;font-size:52px;font-weight:900;"
        "font-family:\"Segoe UI\",sans-serif;letter-spacing:-2px;'>▶ NTH</span>"
        "<span style='color:#ffffff;font-size:52px;font-weight:900;"
        "font-family:\"Segoe UI\",sans-serif;letter-spacing:-2px;'>TUBE</span>");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("background:transparent;");
    logoRow->addWidget(logoLabel);
    contentLayout->addLayout(logoRow);
    contentLayout->addSpacing(10);

    // Tagline
    auto *tagRow = new QHBoxLayout;
    tagRow->setAlignment(Qt::AlignHCenter);
    auto *tagLabel = new QLabel("watch anything, own nothing");
    tagLabel->setAlignment(Qt::AlignCenter);
    tagLabel->setStyleSheet(
        "color:rgba(255,255,255,0.35); font-size:13px; letter-spacing:3px;"
        "font-family:'Segoe UI',sans-serif; background:transparent;");
    tagRow->addWidget(tagLabel);
    contentLayout->addLayout(tagRow);
    contentLayout->addSpacing(44);

    // Search row
    auto *searchRow = new QHBoxLayout;
    searchRow->setAlignment(Qt::AlignHCenter);
    searchRow->setSpacing(10);

    m_homeSearch = new QLineEdit;
    m_homeSearch->setPlaceholderText("Search videos, music, anything...");
    m_homeSearch->setFixedHeight(48);
    m_homeSearch->setFixedWidth(580);
    m_homeSearch->setStyleSheet(R"(
        QLineEdit {
            background:rgba(255,255,255,0.08);
            color:#f1f1f1;
            border:1px solid rgba(255,255,255,0.2);
            border-radius:24px;
            padding:10px 24px;
            font-size:14px;
            font-family:'Segoe UI','Noto Sans',sans-serif;
        }
        QLineEdit:focus {
            background:rgba(255,255,255,0.13);
            border-color:#ff4444;
        }
    )");
    connect(m_homeSearch, &QLineEdit::returnPressed, this, &YoutubeTab::onHomeSearch);

    m_homeSearchBtn = new QPushButton("Search");
    m_homeSearchBtn->setFixedHeight(48);
    m_homeSearchBtn->setFixedWidth(110);
    m_homeSearchBtn->setCursor(Qt::PointingHandCursor);
    m_homeSearchBtn->setStyleSheet(R"(
        QPushButton {
            background:#ff4444; color:#fff;
            border:none; border-radius:24px;
            font-size:14px; font-weight:bold;
            font-family:'Segoe UI',sans-serif;
        }
        QPushButton:hover { background:#cc2222; }
    )");
    connect(m_homeSearchBtn, &QPushButton::clicked, this, &YoutubeTab::onHomeSearch);

    searchRow->addWidget(m_homeSearch);
    searchRow->addWidget(m_homeSearchBtn);
    contentLayout->addLayout(searchRow);
    contentLayout->addStretch(3);

    // Hint
    auto *hintRow = new QHBoxLayout;
    hintRow->setAlignment(Qt::AlignHCenter);
    auto *hint = new QLabel("double-click a card to play  ·  click + Watch Next to queue");
    hint->setStyleSheet(
        "color:rgba(255,255,255,0.18); font-size:11px; letter-spacing:1px;"
        "font-family:monospace; background:transparent;");
    hintRow->addWidget(hint);
    contentLayout->addLayout(hintRow);
    contentLayout->addSpacing(12);

    root->addWidget(bgContainer, 1);

    // Mini player — hidden by default, shown when queue is active
    m_miniPlayer = new MiniPlayer(m_homePage);
    m_miniPlayer->hide();
    root->addWidget(m_miniPlayer);

    connect(m_miniPlayer, &MiniPlayer::requestNext,   this, &YoutubeTab::onNextInQueue);
    connect(m_miniPlayer, &MiniPlayer::requestPrev,   this, &YoutubeTab::onPrevInQueue);
    connect(m_miniPlayer, &MiniPlayer::requestToggleLoop, this, &YoutubeTab::onToggleLoop);
    connect(m_miniPlayer, &MiniPlayer::requestExpand, this, [this]() {
        m_pages->setCurrentIndex(1);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
//  Results Page
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::buildResultsPage() {
    m_resultsPage = new QWidget;
    m_resultsPage->setObjectName("resultsPage");
    m_resultsPage->setStyleSheet(R"(
        QWidget#resultsPage {
            background-image: url(:/icons/elysia.jpeg);
            background-repeat: no-repeat;
            background-position: right center;
            background-color: #0f0f0f;
        }
    )");

    auto *root = new QVBoxLayout(m_resultsPage);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    auto *topBar = new QWidget;
    topBar->setFixedHeight(56);
    topBar->setStyleSheet("background:#0f0f0f; border-bottom:1px solid #1e1e1e;");
    auto *tbl = new QHBoxLayout(topBar);
    tbl->setContentsMargins(16,0,16,0);
    tbl->setSpacing(12);

    auto *miniLogo = new QLabel;
    miniLogo->setTextFormat(Qt::RichText);
    miniLogo->setText(
        "<span style='color:#ff4444;font-size:18px;font-weight:900;"
        "font-family:\"Segoe UI\",sans-serif;'>▶ NTH</span>"
        "<span style='color:#fff;font-size:18px;font-weight:900;"
        "font-family:\"Segoe UI\",sans-serif;'>TUBE</span>");
    miniLogo->setStyleSheet("background:transparent;");

    m_backBtn = new QPushButton("← Home", topBar);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setStyleSheet(R"(
        QPushButton {
            background:transparent; color:#aaa;
            border:1px solid #333; border-radius:4px;
            font-size:11px; padding:5px 12px;
            font-family:'Segoe UI',sans-serif;
        }
        QPushButton:hover { color:#fff; border-color:#555; }
    )");
    connect(m_backBtn, &QPushButton::clicked, this, &YoutubeTab::onBackHome);

    m_topSearch = new QLineEdit(topBar);
    m_topSearch->setPlaceholderText("Search...");
    m_topSearch->setFixedHeight(38);
    m_topSearch->setStyleSheet(R"(
        QLineEdit {
            background:#121212; color:#f1f1f1;
            border:1px solid #303030; border-radius:19px;
            padding:6px 18px; font-size:13px;
            font-family:'Segoe UI','Noto Sans',sans-serif;
        }
        QLineEdit:focus { border-color:#ff4444; }
    )");
    connect(m_topSearch, &QLineEdit::returnPressed, this, &YoutubeTab::onTopSearch);

    m_topSearchBtn = new QPushButton("🔍", topBar);
    m_topSearchBtn->setFixedSize(38,38);
    m_topSearchBtn->setCursor(Qt::PointingHandCursor);
    m_topSearchBtn->setStyleSheet(R"(
        QPushButton {
            background:#222; color:#fff;
            border:1px solid #333; border-radius:19px; font-size:14px;
        }
        QPushButton:hover { background:#ff4444; border-color:#ff4444; }
    )");
    connect(m_topSearchBtn, &QPushButton::clicked, this, &YoutubeTab::onTopSearch);

    m_statusLabel = new QLabel("", topBar);
    m_statusLabel->setFixedWidth(200);
    m_statusLabel->setStyleSheet(
        "color:#555; font-size:11px; font-family:monospace; background:transparent;");

    tbl->addWidget(miniLogo);
    tbl->addWidget(m_backBtn);
    tbl->addSpacing(8);
    tbl->addWidget(m_topSearch, 1);
    tbl->addWidget(m_topSearchBtn);
    tbl->addWidget(m_statusLabel);
    root->addWidget(topBar);

    // ── Body splitter: grid | player+queue ───────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background:#1e1e1e; }");

    // LEFT: card grid
    m_gridScroll = new QScrollArea;
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridScroll->setStyleSheet("background:rgba(15,15,15,0.85); border:none;");
    m_gridScroll->setMinimumWidth(280);

    m_gridContainer = new QWidget;
    m_gridContainer->setStyleSheet("background:transparent;");
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(16,16,16,16);
    m_gridLayout->setSpacing(14);
    m_gridLayout->setAlignment(Qt::AlignTop|Qt::AlignLeft);
    m_gridScroll->setWidget(m_gridContainer);

    // RIGHT: player + queue stacked vertically
    auto *rightPanel = new QWidget;
    rightPanel->setStyleSheet("background:rgba(15,15,15,0.92);");
    auto *rpl = new QVBoxLayout(rightPanel);
    rpl->setContentsMargins(0,0,0,0);
    rpl->setSpacing(0);

    // Player
    m_playerStack = new QStackedWidget(rightPanel);
    m_playerStack->setMinimumHeight(280);
    m_playerStack->setStyleSheet("background:#000;");

    auto *placeholder = new QWidget(m_playerStack);
    placeholder->setStyleSheet("background:#0a0a0a;");
    auto *phL = new QVBoxLayout(placeholder);
    phL->setAlignment(Qt::AlignCenter);
    auto *phIcon = new QLabel("▶");
    phIcon->setAlignment(Qt::AlignCenter);
    phIcon->setStyleSheet("color:#1e1e1e; font-size:64px; background:transparent;");
    auto *phText = new QLabel("double-click a card to play");
    phText->setAlignment(Qt::AlignCenter);
    phText->setStyleSheet(
        "color:#333; font-size:12px; letter-spacing:1px;"
        "font-family:'Segoe UI',sans-serif; background:transparent;");
    phL->addWidget(phIcon);
    phL->addWidget(phText);

    m_player = new QWebEngineView(m_playerStack);
    m_playerStack->addWidget(placeholder);
    m_playerStack->addWidget(m_player);
    m_playerStack->setCurrentIndex(0);

    // Info bar below player
    auto *infoArea = new QWidget(rightPanel);
    infoArea->setStyleSheet("background:#0f0f0f; border-top:1px solid #1e1e1e;");
    auto *il = new QVBoxLayout(infoArea);
    il->setContentsMargins(14,12,14,10);
    il->setSpacing(5);

    m_titleLabel = new QLabel("", infoArea);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setMaximumHeight(42);
    m_titleLabel->setStyleSheet(
        "color:#f1f1f1; font-size:14px; font-weight:bold;"
        "font-family:'Segoe UI','Noto Sans',sans-serif; background:transparent;");

    m_metaLabel = new QLabel("", infoArea);
    m_metaLabel->setStyleSheet(
        "color:#aaa; font-size:11px;"
        "font-family:'Segoe UI','Noto Sans',sans-serif; background:transparent;");

    m_descView = new QTextEdit(infoArea);
    m_descView->setReadOnly(true);
    m_descView->setFixedHeight(48);
    m_descView->setPlaceholderText("description...");

    // Controls row
    auto *ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(6);

    m_streamCombo = new QComboBox(infoArea);
    m_streamCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_streamCombo->setFixedHeight(34);

    m_streamBtn   = makeBtn("▶  Stream",  "#ff4444", "transparent", infoArea);
    m_downloadBtn = makeBtn("↓  Save",    "#ffaa00", "transparent", infoArea);
    m_streamBtn->setFixedHeight(34);
    m_downloadBtn->setFixedHeight(34);
    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    connect(m_streamBtn,   &QPushButton::clicked, this, &YoutubeTab::onStream);
    connect(m_downloadBtn, &QPushButton::clicked, this, &YoutubeTab::onDownload);

    // Prev / Next / Loop
    auto mkNavBtn = [&](const QString &lbl) {
        auto *b = new QPushButton(lbl, infoArea);
        b->setFixedSize(34,34);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(R"(
            QPushButton {
                background:transparent; color:#aaa;
                border:1px solid #333; border-radius:17px;
                font-size:16px;
            }
            QPushButton:hover { color:#fff; border-color:#ff4444; }
            QPushButton:disabled { color:#2a2a2a; border-color:#1a1a1a; }
        )");
        return b;
    };
    m_prevBtn = mkNavBtn("⏮");
    m_nextBtn = mkNavBtn("⏭");
    m_loopBtn = mkNavBtn("⟳");
    m_prevBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    connect(m_prevBtn, &QPushButton::clicked, this, &YoutubeTab::onPrevInQueue);
    connect(m_nextBtn, &QPushButton::clicked, this, &YoutubeTab::onNextInQueue);
    connect(m_loopBtn, &QPushButton::clicked, this, &YoutubeTab::onToggleLoop);

    ctrlRow->addWidget(m_streamCombo, 1);
    ctrlRow->addWidget(m_streamBtn);
    ctrlRow->addWidget(m_downloadBtn);
    ctrlRow->addWidget(m_prevBtn);
    ctrlRow->addWidget(m_nextBtn);
    ctrlRow->addWidget(m_loopBtn);

    // Progress
    auto *progRow = new QHBoxLayout;
    progRow->setSpacing(8);
    m_progress = new QProgressBar(infoArea);
    m_progress->setRange(0,100);
    m_progress->setFixedHeight(5);
    m_progress->hide();
    m_progressLabel = new QLabel("", infoArea);
    m_progressLabel->setStyleSheet("color:#555; font-size:10px; background:transparent;");
    progRow->addWidget(m_progress,1);
    progRow->addWidget(m_progressLabel);

    il->addWidget(m_titleLabel);
    il->addWidget(m_metaLabel);
    il->addWidget(m_descView);
    il->addLayout(ctrlRow);
    il->addLayout(progRow);

    // Queue panel
    auto *queueHeader = new QWidget(rightPanel);
    queueHeader->setFixedHeight(32);
    queueHeader->setStyleSheet(
        "background:#111; border-top:1px solid #1e1e1e;");
    auto *qhl = new QHBoxLayout(queueHeader);
    qhl->setContentsMargins(14,0,14,0);
    m_queueLabel = new QLabel("QUEUE  ( 0 )", queueHeader);
    m_queueLabel->setStyleSheet(
        "color:#555; font-size:10px; font-weight:bold; letter-spacing:1px;"
        "font-family:monospace; background:transparent;");
    auto *clearBtn = new QPushButton("clear", queueHeader);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(R"(
        QPushButton {
            background:transparent; color:#333;
            border:none; font-size:10px;
            font-family:monospace;
        }
        QPushButton:hover { color:#ff4444; }
    )");
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_queue.clear();
        m_queueIndex = -1;
        updateQueuePanel();
        updateNavButtons();
        m_autoNextTimer->stop();
        m_miniPlayer->hide();
    });
    qhl->addWidget(m_queueLabel);
    qhl->addStretch();
    qhl->addWidget(clearBtn);

    m_queueList = new QListWidget(rightPanel);
    m_queueList->setFixedHeight(120);
    m_queueList->setStyleSheet(
        "QListWidget { background:#0a0a0a; border:none; }"
        "QListWidget::item { padding:5px 12px; border-bottom:1px solid #141414; color:#aaa; }"
        "QListWidget::item:selected { background:#1a0000; color:#ff4444; }");
    connect(m_queueList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        int idx = item->data(Qt::UserRole).toInt();
        playQueueIndex(idx);
    });

    rpl->addWidget(m_playerStack, 1);
    rpl->addWidget(infoArea);
    rpl->addWidget(queueHeader);
    rpl->addWidget(m_queueList);

    splitter->addWidget(m_gridScroll);
    splitter->addWidget(rightPanel);
    splitter->setSizes({480,760});
    root->addWidget(splitter,1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Navigation
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onHomeSearch() {
    QString q = m_homeSearch->text().trimmed();
    if (q.isEmpty()) return;
    m_topSearch->setText(q);
    m_pages->setCurrentIndex(1);
    doSearch(q);
}

void YoutubeTab::onTopSearch() {
    QString q = m_topSearch->text().trimmed();
    if (q.isEmpty()) return;
    doSearch(q);
}

void YoutubeTab::onBackHome() {
    m_homeSearch->clear();
    m_pages->setCurrentIndex(0);
    // Mini player shows if something is in queue
    updateMiniPlayer();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Search
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::doSearch(const QString &q) {
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget(); delete item;
    }
    m_results.clear();
    m_searchBuf.clear();
    setStatus("searching...", "#ff4444");
    runBridge(m_searchProc, {"search", q},
        SLOT(onSearchOutput()), SLOT(onSearchFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onSearchOutput() {
    m_searchBuf += QString::fromUtf8(m_searchProc->readAllStandardOutput());
}

void YoutubeTab::onSearchFinished(int code, QProcess::ExitStatus) {
    if (code != 0) { setStatus("search failed", "#ff4444"); return; }
    QJsonDocument doc = QJsonDocument::fromJson(m_searchBuf.toUtf8());
    if (!doc.isArray()) { setStatus("bad response", "#ff4444"); return; }
    populateResults(doc.array());
}

void YoutubeTab::populateResults(const QJsonArray &arr) {
    m_results.clear();
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget(); delete item;
    }

    int col=0, row=0;
    const int COLS=2;

    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        YTResult r;
        r.id        = o["id"].toString();
        r.url       = o["url"].toString();
        r.title     = o["title"].toString();
        r.uploader  = o["uploader"].toString();
        r.thumbnail = o["thumbnail"].toString();
        r.duration  = o["duration"].toInt();
        r.views     = (qint64)o["views"].toDouble();
        m_results.append(r);

        auto *card = new VideoCard(r, m_results.size()-1, m_nam, m_gridContainer);
        connect(card, &VideoCard::activated,    this, &YoutubeTab::onCardActivated);
        connect(card, &VideoCard::addToQueue,   this, &YoutubeTab::onCardAddToQueue);
        m_gridLayout->addWidget(card, row, col);
        col++;
        if (col >= COLS) { col=0; row++; }
    }
    m_gridLayout->setRowStretch(row+1, 1);
    setStatus(QString("%1 results").arg(m_results.size()), "#00cc66");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Queue management
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onCardAddToQueue(int idx) {
    if (idx < 0 || idx >= m_results.size()) return;
    QueueEntry entry;
    entry.result = m_results[idx];
    entry.ready  = false;
    m_queue.append(entry);
    updateQueuePanel();

    // If nothing is currently playing, start it
    if (m_queueIndex < 0) {
        playQueueIndex(m_queue.size()-1);
    } else {
        setStatus(QString("added to queue: %1").arg(entry.result.title.left(30)), "#ffaa00");
        // Pre-fetch stream info for the new item
        fetchQueueItemInfo(m_queue.size()-1);
    }
}

void YoutubeTab::fetchQueueItemInfo(int qIdx) {
    if (qIdx < 0 || qIdx >= m_queue.size()) return;
    m_pendingQueueIdx = qIdx;
    m_queueInfoBuf.clear();
    runBridge(m_queueInfoProc, {"info", m_queue[qIdx].result.url},
        SLOT(onQueueInfoOutput()), SLOT(onQueueInfoFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onQueueInfoOutput() {
    m_queueInfoBuf += QString::fromUtf8(m_queueInfoProc->readAllStandardOutput());
}

void YoutubeTab::onQueueInfoFinished(int code, QProcess::ExitStatus) {
    if (code != 0 || m_pendingQueueIdx < 0 || m_pendingQueueIdx >= m_queue.size()) return;
    QJsonDocument doc = QJsonDocument::fromJson(m_queueInfoBuf.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    // Pick best muxed stream
    QJsonArray streams = obj["streams"].toArray();
    if (streams.isEmpty()) return;

    // prefer 720p muxed, fallback to first
    QString bestUrl, bestQuality;
    for (const auto &v : streams) {
        QJsonObject s = v.toObject();
        if (s["type"].toString() == "muxed") {
            if (bestUrl.isEmpty() || s["quality"].toString() == "720p") {
                bestUrl     = s["url"].toString();
                bestQuality = s["quality"].toString();
            }
        }
    }
    if (bestUrl.isEmpty()) {
        // fallback to first stream
        QJsonObject s = streams[0].toObject();
        bestUrl     = s["url"].toString();
        bestQuality = s["quality"].toString();
    }

    m_queue[m_pendingQueueIdx].streamUrl = bestUrl;
    m_queue[m_pendingQueueIdx].quality   = bestQuality;
    m_queue[m_pendingQueueIdx].ready     = true;
    updateQueuePanel();
}

void YoutubeTab::playQueueIndex(int idx) {
    if (idx < 0 || idx >= m_queue.size()) return;
    m_queueIndex = idx;
    m_currentResult = m_queue[idx].result;

    m_titleLabel->setText(m_currentResult.title);
    m_metaLabel->setText(
        m_currentResult.uploader + "  ·  " +
        formatDuration(m_currentResult.duration) + "  ·  " +
        formatViews(m_currentResult.views) + " views");
    m_descView->setPlainText("loading...");
    m_streamCombo->clear();
    m_streamCombo->addItem("loading streams...");
    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_infoBuf.clear();

    updateQueuePanel();
    updateMiniPlayer();
    updateNavButtons();

    setStatus("loading: " + m_currentResult.title.left(30), "#ffaa00");
    m_pages->setCurrentIndex(1);

    // If stream is already ready (pre-fetched), play directly
    if (m_queue[idx].ready && !m_queue[idx].streamUrl.isEmpty()) {
        m_player->setUrl(QUrl(m_queue[idx].streamUrl));
        m_playerStack->setCurrentIndex(1);
        m_autoNextTimer->start();
        setStatus("playing: " + m_currentResult.title.left(30), "#00cc66");
        // Still fetch info for the combo/description
        runBridge(m_infoProc, {"info", m_currentResult.url},
            SLOT(onInfoOutput()), SLOT(onInfoFinished(int,QProcess::ExitStatus)));
    } else {
        runBridge(m_infoProc, {"info", m_currentResult.url},
            SLOT(onInfoOutput()), SLOT(onInfoFinished(int,QProcess::ExitStatus)));
    }
}

void YoutubeTab::onNextInQueue() {
    if (m_queue.isEmpty()) return;
    int next = m_looping
        ? m_queueIndex   // loop: replay same
        : m_queueIndex + 1;
    if (next >= m_queue.size()) {
        if (m_looping) next = 0;
        else { setStatus("end of queue", "#aaa"); m_autoNextTimer->stop(); return; }
    }
    playQueueIndex(next);
    // Pre-fetch the one after
    if (next+1 < m_queue.size() && !m_queue[next+1].ready)
        fetchQueueItemInfo(next+1);
}

void YoutubeTab::onPrevInQueue() {
    if (m_queueIndex <= 0) return;
    playQueueIndex(m_queueIndex - 1);
}

void YoutubeTab::onToggleLoop() {
    m_looping = !m_looping;
    QString style = m_looping
        ? R"(QPushButton { background:#ff4444; color:#fff;
              border:1px solid #ff4444; border-radius:17px; font-size:16px; }
             QPushButton:hover { background:#cc2222; })"
        : R"(QPushButton { background:transparent; color:#aaa;
              border:1px solid #333; border-radius:17px; font-size:16px; }
             QPushButton:hover { color:#fff; border-color:#ff4444; })";
    m_loopBtn->setStyleSheet(style);
    m_miniPlayer->m_loopBtn->setStyleSheet(
        m_looping
        ? "QPushButton{background:#ff4444;color:#fff;border:1px solid #ff4444;"
          "border-radius:16px;font-size:14px;}"
        : "QPushButton{background:transparent;color:#aaa;border:1px solid #333;"
          "border-radius:16px;font-size:14px;}"
          "QPushButton:hover{color:#fff;border-color:#ff4444;}");
    setStatus(m_looping ? "loop ON" : "loop OFF", m_looping ? "#ff4444" : "#aaa");
}

void YoutubeTab::onCheckAutoNext() {
    // Ask the page if the video element has ended
    m_player->page()->runJavaScript(
        "var v = document.querySelector('video');"
        "v ? v.ended : false;",
        [this](const QVariant &result) {
            if (result.toBool()) {
                m_autoNextTimer->stop();
                onNextInQueue();
            }
        });
}

void YoutubeTab::updateQueuePanel() {
    m_queueList->clear();
    for (int i = 0; i < m_queue.size(); ++i) {
        const auto &e = m_queue[i];
        QString prefix = (i == m_queueIndex) ? "▶  " : QString("%1.  ").arg(i+1);
        QString ready  = e.ready ? "" : " ⌛";
        auto *item = new QListWidgetItem(prefix + e.result.title.left(45) + ready);
        item->setData(Qt::UserRole, i);
        if (i == m_queueIndex)
            item->setForeground(QColor("#ff4444"));
        m_queueList->addItem(item);
    }
    m_queueLabel->setText(QString("QUEUE  ( %1 )").arg(m_queue.size()));
}

void YoutubeTab::updateMiniPlayer() {
    if (m_queueIndex < 0 || m_queue.isEmpty()) {
        m_miniPlayer->hide();
        return;
    }
    m_miniPlayer->show();
    m_miniPlayer->setNowPlaying(
        m_queue[m_queueIndex].result.title,
        m_queue[m_queueIndex].result.uploader);
    m_miniPlayer->setQueueSize(m_queue.size(), m_queueIndex);
}

void YoutubeTab::updateNavButtons() {
    m_prevBtn->setEnabled(m_queueIndex > 0);
    m_nextBtn->setEnabled(m_queueIndex >= 0 &&
                          (m_queueIndex < m_queue.size()-1 || m_looping));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Card activated (double-click = play NOW, add to queue at current pos)
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onCardActivated(int idx) {
    if (idx < 0 || idx >= m_results.size()) return;

    // Insert at front of remaining queue (after current)
    QueueEntry entry;
    entry.result = m_results[idx];
    entry.ready  = false;

    int insertAt = (m_queueIndex >= 0) ? m_queueIndex + 1 : 0;
    m_queue.insert(insertAt, entry);
    playQueueIndex(insertAt);
    // Pre-fetch next
    if (insertAt+1 < m_queue.size()) fetchQueueItemInfo(insertAt+1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Info fetch (for current playing video — fills combo + description)
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onInfoOutput() {
    m_infoBuf += QString::fromUtf8(m_infoProc->readAllStandardOutput());
}

void YoutubeTab::onInfoFinished(int code, QProcess::ExitStatus) {
    if (code != 0) { setStatus("failed to get streams", "#ff4444"); return; }
    QJsonDocument doc = QJsonDocument::fromJson(m_infoBuf.toUtf8());
    if (!doc.isObject()) { setStatus("bad stream response", "#ff4444"); return; }
    populateStreams(doc.object());
}

void YoutubeTab::populateStreams(const QJsonObject &obj) {
    m_streams.clear();
    m_streamCombo->clear();

    QString rawDesc = obj["description"].toString().left(1000);
    rawDesc.remove(QRegularExpression("<[^>]*>"));
    rawDesc.replace("&amp;","&").replace("&lt;","<").replace("&gt;",">")
           .replace("&quot;","\"").replace("&#39;","'");
    m_descView->setPlainText(rawDesc.trimmed().left(400));

    auto parse = [&](const QJsonArray &arr) {
        for (const auto &v : arr) {
            QJsonObject s = v.toObject();
            YTStream st;
            st.type    = s["type"].toString();
            st.quality = s["quality"].toString();
            st.format  = s["format"].toString();
            st.url     = s["url"].toString();
            st.bitrate = s["bitrate"].toInt();
            st.fps     = s["fps"].toInt();
            m_streams.append(st);
            QString label;
            if (st.type=="audio")
                label=QString("[AUDIO] %1kbps %2").arg(st.bitrate).arg(st.format.toUpper());
            else if (st.type=="muxed")
                label=QString("[VIDEO+AUDIO] %1 %2 %3fps").arg(st.quality,st.format.toUpper()).arg(st.fps);
            else
                label=QString("[VIDEO ONLY] %1 %2 %3fps").arg(st.quality,st.format.toUpper()).arg(st.fps);
            m_streamCombo->addItem(label, m_streams.size()-1);
        }
    };
    parse(obj["streams"].toArray());
    parse(obj["audio"].toArray());
    parse(obj["videoOnly"].toArray());

    m_streamBtn->setEnabled(true);
    m_downloadBtn->setEnabled(true);

    // Auto-stream first muxed if we came from queue
    if (m_queueIndex >= 0 && m_playerStack->currentIndex()==0) {
        onStream();
    }

    setStatus(QString("ready — %1 streams").arg(m_streams.size()), "#00cc66");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stream / Download
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onStream() {
    int idx = m_streamCombo->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;
    const YTStream &st = m_streams[idx];

    // Update queue entry stream url
    if (m_queueIndex >= 0 && m_queueIndex < m_queue.size()) {
        m_queue[m_queueIndex].streamUrl = st.url;
        m_queue[m_queueIndex].quality   = st.quality;
        m_queue[m_queueIndex].ready     = true;
    }

    m_player->setUrl(QUrl(st.url));
    m_playerStack->setCurrentIndex(1);
    m_autoNextTimer->start();
    updateMiniPlayer();
    setStatus("streaming: " + st.quality, "#00cc66");
}

void YoutubeTab::onDownload() {
    int idx = m_streamCombo->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;
    const YTStream &st = m_streams[idx];

    QString safe = m_currentResult.title;
    safe.replace(QRegularExpression("[^a-zA-Z0-9 _\\-]"),"");
    safe = safe.simplified().replace(' ','_').left(60);
    if (safe.isEmpty()) safe = "video";

    QString ext = st.type=="audio" ? st.format : "mp4";
    ext = ext.split(' ').first().toLower();
    QString def = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                  + "/" + safe + "." + ext;

    QString outPath = QFileDialog::getSaveFileName(
        this,"Save As",def,
        QString("%1 (*.%1);;All Files (*)").arg(ext));
    if (outPath.isEmpty()) return;

    m_progress->setValue(0);
    m_progress->show();
    m_progressLabel->setText("0%");
    m_downloadBtn->setEnabled(false);
    m_dlBuf.clear();
    setStatus("downloading...","#ffaa00");

    runBridge(m_dlProc, {"download", m_currentResult.url, st.url, outPath},
        SLOT(onDownloadOutput()), SLOT(onDownloadFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onDownloadOutput() {
    m_dlBuf += QString::fromUtf8(m_dlProc->readAllStandardOutput());
    QStringList lines = m_dlBuf.split('\n', Qt::SkipEmptyParts);
    if (!m_dlBuf.endsWith('\n') && !lines.isEmpty())
        m_dlBuf = lines.takeLast();
    else m_dlBuf.clear();

    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
        if (!doc.isObject()) continue;
        int pct = doc.object()["progress"].toInt(-1);
        if (pct >= 0) {
            m_progress->setValue(pct);
            m_progressLabel->setText(QString("%1%").arg(pct));
        }
    }
}

void YoutubeTab::onDownloadFinished(int code, QProcess::ExitStatus) {
    m_downloadBtn->setEnabled(true);
    QStringList lines = m_dlBuf.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
        if (!doc.isObject()) continue;
        if (doc.object()["done"].toBool()) {
            m_progress->setValue(100);
            m_progressLabel->setText("done");
            setStatus("saved: " + doc.object()["path"].toString(), "#00cc66");
            return;
        }
    }
    if (code != 0) { m_progress->hide(); setStatus("download failed","#ff4444"); }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Bridge
// ═════════════════════════════════════════════════════════════════════════════
QString YoutubeTab::jarPath() const {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/newpipe-bridge.jar",
        QDir::currentPath() + "/newpipe-bridge/build/libs/newpipe-bridge-1.0.0.jar",
        QDir::currentPath() + "/newpipe-bridge.jar",
    };
    for (auto &c : candidates)
        if (QFile::exists(c)) return c;
    return candidates.first();
}

void YoutubeTab::runBridge(QProcess *&proc, const QStringList &args,
                            const char *readySlot, const char *finishedSlot) {
    if (proc && proc->state() != QProcess::NotRunning) {
        proc->kill(); proc->waitForFinished(1000);
    }
    delete proc;
    proc = new QProcess(this);
    QStringList javaArgs = {"-jar", jarPath()};
    javaArgs << args;
    connect(proc, SIGNAL(readyReadStandardOutput()), this, readySlot);
    connect(proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, finishedSlot);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start("java", javaArgs);
    if (!proc->waitForStarted(5000))
        setStatus("java not found — install JDK 11+","#ff4444");
}

void YoutubeTab::setStatus(const QString &msg, const QString &color) {
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("color:%1; font-size:11px; font-family:monospace;"
                "background:transparent;").arg(color));
}

QString YoutubeTab::formatDuration(qint64 s) const {
    if (s<=0) return "LIVE";
    if (s>=3600)
        return QString("%1:%2:%3")
            .arg(s/3600).arg((s%3600)/60,2,10,QChar('0')).arg(s%60,2,10,QChar('0'));
    return QString("%1:%2").arg(s/60).arg(s%60,2,10,QChar('0'));
}

QString YoutubeTab::formatViews(qint64 v) const {
    if (v>=1'000'000'000) return QString::number(v/1'000'000'000.0,'f',1)+"B";
    if (v>=1'000'000)     return QString::number(v/1'000'000.0,'f',1)+"M";
    if (v>=1'000)         return QString::number(v/1'000.0,'f',0)+"K";
    return QString::number(v);
}