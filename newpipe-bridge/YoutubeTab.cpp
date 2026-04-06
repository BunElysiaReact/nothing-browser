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
    setStyleSheet(R"(
        VideoCard {
            background: #181818;
            border-radius: 8px;
        }
    )");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 8);
    root->setSpacing(6);

    // ── Thumbnail ─────────────────────────────────────────────────────────────
    auto *thumbWrap = new QWidget(this);
    thumbWrap->setFixedSize(220, 124);
    thumbWrap->setStyleSheet(
        "background: #0f0f0f;"
        "border-radius: 8px 8px 0 0;");

    m_thumb = new QLabel(thumbWrap);
    m_thumb->setGeometry(0, 0, 220, 124);
    m_thumb->setAlignment(Qt::AlignCenter);
    m_thumb->setStyleSheet(
        "background: #0f0f0f; color: #333;"
        "font-size: 28px; border-radius: 8px 8px 0 0;");
    m_thumb->setText("▶");

    // Duration badge (bottom-right of thumb)
    m_duration = new QLabel(thumbWrap);
    m_duration->setAlignment(Qt::AlignCenter);
    m_duration->setStyleSheet(
        "background: rgba(0,0,0,0.85); color: #fff;"
        "font-family: 'JetBrains Mono', monospace; font-size: 10px;"
        "font-weight: bold; border-radius: 3px;"
        "padding: 1px 5px;");
    m_duration->setText(r.duration <= 0 ? "LIVE" :
        (r.duration >= 3600
            ? QString("%1:%2:%3").arg(r.duration/3600).arg((r.duration%3600)/60,2,10,QChar('0')).arg(r.duration%60,2,10,QChar('0'))
            : QString("%1:%2").arg(r.duration/60).arg(r.duration%60,2,10,QChar('0'))));
    m_duration->adjustSize();
    m_duration->move(220 - m_duration->width() - 6,
                     124 - m_duration->height() - 6);

    // ── Text block ────────────────────────────────────────────────────────────
    auto *textArea = new QWidget(this);
    textArea->setStyleSheet("background: transparent;");
    auto *tl = new QVBoxLayout(textArea);
    tl->setContentsMargins(10, 2, 10, 0);
    tl->setSpacing(2);

    m_title = new QLabel(r.title, textArea);
    m_title->setWordWrap(true);
    m_title->setMaximumHeight(36);
    m_title->setStyleSheet(
        "color: #f1f1f1; font-size: 12px; font-weight: bold;"
        "font-family: 'Segoe UI', 'Noto Sans', sans-serif;"
        "background: transparent; line-height: 1.3;");

    m_channel = new QLabel(r.uploader, textArea);
    m_channel->setStyleSheet(
        "color: #aaaaaa; font-size: 11px;"
        "font-family: 'Segoe UI', 'Noto Sans', sans-serif;"
        "background: transparent;");

    QString viewStr;
    qint64 v = r.views;
    if (v >= 1'000'000'000) viewStr = QString::number(v/1'000'000'000.0,'f',1)+"B views";
    else if (v >= 1'000'000) viewStr = QString::number(v/1'000'000.0,'f',1)+"M views";
    else if (v >= 1'000)     viewStr = QString::number(v/1'000.0,'f',0)+"K views";
    else                     viewStr = QString::number(v)+" views";

    m_views = new QLabel(viewStr, textArea);
    m_views->setStyleSheet(
        "color: #aaaaaa; font-size: 11px;"
        "font-family: 'Segoe UI', 'Noto Sans', sans-serif;"
        "background: transparent;");

    tl->addWidget(m_title);
    tl->addWidget(m_channel);
    tl->addWidget(m_views);

    root->addWidget(thumbWrap);
    root->addWidget(textArea);

    setFixedHeight(220);

    // ── Fetch thumbnail ───────────────────────────────────────────────────────
    if (!r.thumbnail.isEmpty()) {
        QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(r.thumbnail)));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            QPixmap px;
            if (!px.loadFromData(reply->readAll())) return;
            px = px.scaled(220, 124, Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation);
            QPixmap cropped(220, 124);
            cropped.fill(Qt::transparent);
            QPainter p(&cropped);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addRoundedRect(0, 0, 220, 124, 8, 8);
            p.setClipPath(path);
            p.drawPixmap((220-px.width())/2, (124-px.height())/2, px);
            m_thumb->setPixmap(cropped);
            m_thumb->setText("");
        });
    }
}

void VideoCard::enterEvent(QEnterEvent *) {
    setStyleSheet("VideoCard { background: #222; border-radius: 8px; }");
}
void VideoCard::leaveEvent(QEvent *) {
    setStyleSheet("VideoCard { background: #181818; border-radius: 8px; }");
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
            background: %2; color: %1;
            border: 1px solid %1;
            border-radius: 4px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px; font-weight: bold;
            padding: 7px 20px;
        }
        QPushButton:hover   { background: %1; color: #0f0f0f; }
        QPushButton:pressed { opacity: 0.8; }
        QPushButton:disabled { color: #333; border-color: #222; background: #111; }
    )").arg(fg, bg));
    return b;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Constructor
// ═════════════════════════════════════════════════════════════════════════════
YoutubeTab::YoutubeTab(QWidget *parent) : QWidget(parent) {
    m_nam = new QNetworkAccessManager(this);

    setStyleSheet(R"(
        QWidget      { background: #0f0f0f; color: #f1f1f1; }
        QScrollArea  { border: none; background: #0f0f0f; }
        QScrollBar:vertical { background: #0f0f0f; width: 6px; border: none; }
        QScrollBar::handle:vertical { background: #333; border-radius: 3px; min-height: 20px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { height: 0; }
        QLineEdit {
            background: #121212; color: #f1f1f1;
            border: 1px solid #303030; border-radius: 20px;
            padding: 8px 18px; font-size: 14px;
            font-family: 'Segoe UI', 'Noto Sans', sans-serif;
        }
        QLineEdit:focus { border-color: #ff4444; }
        QComboBox {
            background: #1e1e1e; color: #f1f1f1;
            border: 1px solid #303030; border-radius: 4px;
            padding: 6px 10px; font-size: 12px;
        }
        QComboBox QAbstractItemView {
            background: #1e1e1e; color: #f1f1f1;
            selection-background-color: #2a0000;
            border: 1px solid #303030;
        }
        QComboBox::drop-down { border: none; }
        QTextEdit {
            background: #111; color: #888; border: none;
            font-size: 12px; padding: 6px;
            font-family: 'Segoe UI', 'Noto Sans', sans-serif;
        }
        QProgressBar {
            background: #222; border: none; border-radius: 3px;
            text-align: center; font-size: 10px; color: #888; height: 6px;
        }
        QProgressBar::chunk { background: #ff4444; border-radius: 3px; }
        QLabel { background: transparent; }
        QSplitter::handle { background: #1e1e1e; }
    )");

    m_pages = new QStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    root->addWidget(m_pages);

    buildHomePage();
    buildResultsPage();

    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_resultsPage);
    m_pages->setCurrentIndex(0);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Home Page  — Elysia bg + centered logo + search
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::buildHomePage() {
    m_homePage = new QWidget;
    m_homePage->setStyleSheet("background: #0f0f0f;");

    auto *root = new QVBoxLayout(m_homePage);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Background: Elysia image fills the whole page ─────────────────────────
    // We use a QLabel as background layer, then overlay content via a layout
    // The trick: set the bg label as a child positioned at (0,0) covering the page.
    // We can't truly layer QLayouts, so we use a container with a stack approach.

    // Outer container that paints the Elysia image as background
    auto *bgContainer = new QWidget(m_homePage);
    bgContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // This stylesheet sets the elysia image as background via Qt resource
    // It will try the resource path first; if not found, will just show dark bg.
    bgContainer->setStyleSheet(R"(
        QWidget#elysiaBackground {
            background-image: url(:/assets/icons/elysia.jpeg);
            background-repeat: no-repeat;
            background-position: center;
            background-color: #0f0f0f;
        }
    )");
    bgContainer->setObjectName("elysiaBackground");

    // Dark overlay on top of the image — gives it the cinematic YouTube feel
    auto *overlay = new QWidget(bgContainer);
    overlay->setStyleSheet(
        "background: rgba(0,0,0,0); "  // transparent — let Qt stylesheet handle
    );
    // We'll paint the overlay via palette
    auto overlayPalette = overlay->palette();
    overlayPalette.setColor(QPalette::Window, QColor(0, 0, 0, 160));
    overlay->setAutoFillBackground(true);
    overlay->setPalette(overlayPalette);

    // Layout the overlay to fill bgContainer
    auto *bgLayout = new QVBoxLayout(bgContainer);
    bgLayout->setContentsMargins(0,0,0,0);
    bgLayout->addWidget(overlay);

    // Layout the content INSIDE the overlay
    auto *contentLayout = new QVBoxLayout(overlay);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addStretch(2);

    // ── Logo ──────────────────────────────────────────────────────────────────
    auto *logoRow = new QHBoxLayout;
    logoRow->setAlignment(Qt::AlignHCenter);

    auto *logoLabel = new QLabel;
    logoLabel->setTextFormat(Qt::RichText);
    logoLabel->setText(
        "<span style='color:#ff4444; font-size:48px; font-weight:900;"
        " font-family:\"Segoe UI\",sans-serif; letter-spacing:-2px;'>▶ NTH</span>"
        "<span style='color:#ffffff; font-size:48px; font-weight:900;"
        " font-family:\"Segoe UI\",sans-serif; letter-spacing:-2px;'>TUBE</span>");
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("background: transparent;");

    logoRow->addWidget(logoLabel);
    contentLayout->addLayout(logoRow);
    contentLayout->addSpacing(8);

    // ── Tagline ───────────────────────────────────────────────────────────────
    auto *tagRow = new QHBoxLayout;
    tagRow->setAlignment(Qt::AlignHCenter);
    auto *tagLabel = new QLabel("watch anything, own nothing");
    tagLabel->setAlignment(Qt::AlignCenter);
    tagLabel->setStyleSheet(
        "color: rgba(255,255,255,0.4); font-size: 13px; letter-spacing: 3px;"
        "font-family: 'Segoe UI', sans-serif; background: transparent;");
    tagRow->addWidget(tagLabel);
    contentLayout->addLayout(tagRow);
    contentLayout->addSpacing(40);

    // ── Search row ────────────────────────────────────────────────────────────
    auto *searchRow = new QHBoxLayout;
    searchRow->setAlignment(Qt::AlignHCenter);
    searchRow->setSpacing(10);

    m_homeSearch = new QLineEdit;
    m_homeSearch->setPlaceholderText("Search videos, music, anything...");
    m_homeSearch->setFixedHeight(46);
    m_homeSearch->setFixedWidth(560);
    m_homeSearch->setStyleSheet(R"(
        QLineEdit {
            background: rgba(255,255,255,0.08);
            color: #f1f1f1;
            border: 1px solid rgba(255,255,255,0.2);
            border-radius: 23px;
            padding: 10px 22px;
            font-size: 14px;
            font-family: 'Segoe UI', 'Noto Sans', sans-serif;
        }
        QLineEdit:focus {
            background: rgba(255,255,255,0.12);
            border-color: #ff4444;
        }
    )");
    connect(m_homeSearch, &QLineEdit::returnPressed, this, &YoutubeTab::onHomeSearch);

    m_homeSearchBtn = makeBtn("Search", "#ff4444", "transparent", nullptr);
    m_homeSearchBtn->setFixedHeight(46);
    m_homeSearchBtn->setFixedWidth(110);
    m_homeSearchBtn->setStyleSheet(R"(
        QPushButton {
            background: #ff4444; color: #fff;
            border: none; border-radius: 23px;
            font-size: 14px; font-weight: bold;
            font-family: 'Segoe UI', sans-serif;
            padding: 0 24px;
        }
        QPushButton:hover { background: #cc2222; }
    )");
    connect(m_homeSearchBtn, &QPushButton::clicked, this, &YoutubeTab::onHomeSearch);

    searchRow->addWidget(m_homeSearch);
    searchRow->addWidget(m_homeSearchBtn);
    contentLayout->addLayout(searchRow);

    contentLayout->addStretch(3);

    // Subtle footer hint
    auto *hintRow = new QHBoxLayout;
    hintRow->setAlignment(Qt::AlignHCenter);
    auto *hint = new QLabel("double-click a result to play");
    hint->setStyleSheet(
        "color: rgba(255,255,255,0.2); font-size: 11px; letter-spacing: 1px;"
        "font-family: monospace; background: transparent;");
    hintRow->addWidget(hint);
    contentLayout->addLayout(hintRow);
    contentLayout->addSpacing(20);

    root->addWidget(bgContainer, 1);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Results Page  — top bar + card grid (left) + player panel (right)
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::buildResultsPage() {
    m_resultsPage = new QWidget;
    auto *root = new QVBoxLayout(m_resultsPage);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    auto *topBar = new QWidget;
    topBar->setFixedHeight(56);
    topBar->setStyleSheet(
        "background: #0f0f0f;"
        "border-bottom: 1px solid #1e1e1e;");
    auto *tbl = new QHBoxLayout(topBar);
    tbl->setContentsMargins(16, 0, 16, 0);
    tbl->setSpacing(12);

    // Logo (small version in top bar)
    auto *miniLogo = new QLabel;
    miniLogo->setTextFormat(Qt::RichText);
    miniLogo->setText(
        "<span style='color:#ff4444; font-size:18px; font-weight:900;"
        " font-family:\"Segoe UI\",sans-serif;'>▶ NTH</span>"
        "<span style='color:#ffffff; font-size:18px; font-weight:900;"
        " font-family:\"Segoe UI\",sans-serif;'>TUBE</span>");
    miniLogo->setStyleSheet("background: transparent;");

    m_backBtn = new QPushButton("← Home", topBar);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent; color: #aaa;
            border: 1px solid #333; border-radius: 4px;
            font-size: 11px; padding: 5px 12px;
            font-family: 'Segoe UI', sans-serif;
        }
        QPushButton:hover { color: #fff; border-color: #555; }
    )");
    connect(m_backBtn, &QPushButton::clicked, this, &YoutubeTab::onBackHome);

    m_topSearch = new QLineEdit(topBar);
    m_topSearch->setPlaceholderText("Search...");
    m_topSearch->setFixedHeight(38);
    m_topSearch->setStyleSheet(R"(
        QLineEdit {
            background: #121212; color: #f1f1f1;
            border: 1px solid #303030; border-radius: 19px;
            padding: 6px 18px; font-size: 13px;
            font-family: 'Segoe UI', 'Noto Sans', sans-serif;
        }
        QLineEdit:focus { border-color: #ff4444; }
    )");
    connect(m_topSearch, &QLineEdit::returnPressed, this, &YoutubeTab::onTopSearch);

    m_topSearchBtn = new QPushButton("🔍", topBar);
    m_topSearchBtn->setFixedSize(38, 38);
    m_topSearchBtn->setCursor(Qt::PointingHandCursor);
    m_topSearchBtn->setStyleSheet(R"(
        QPushButton {
            background: #222; color: #fff;
            border: 1px solid #333; border-radius: 19px;
            font-size: 14px;
        }
        QPushButton:hover { background: #ff4444; border-color: #ff4444; }
    )");
    connect(m_topSearchBtn, &QPushButton::clicked, this, &YoutubeTab::onTopSearch);

    m_statusLabel = new QLabel("", topBar);
    m_statusLabel->setFixedWidth(180);
    m_statusLabel->setStyleSheet(
        "color: #555; font-size: 11px; font-family: monospace;"
        "background: transparent;");

    tbl->addWidget(miniLogo);
    tbl->addWidget(m_backBtn);
    tbl->addSpacing(8);
    tbl->addWidget(m_topSearch, 1);
    tbl->addWidget(m_topSearchBtn);
    tbl->addWidget(m_statusLabel);

    root->addWidget(topBar);

    // ── Body: splitter = card grid | player panel ─────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background: #1e1e1e; }");

    // ── LEFT: scrollable card grid ────────────────────────────────────────────
    m_gridScroll = new QScrollArea;
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridScroll->setStyleSheet("background: #0f0f0f; border: none;");
    m_gridScroll->setMinimumWidth(280);

    m_gridContainer = new QWidget;
    m_gridContainer->setStyleSheet("background: #0f0f0f;");
    m_gridLayout = new QGridLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(16, 16, 16, 16);
    m_gridLayout->setSpacing(14);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_gridScroll->setWidget(m_gridContainer);

    // ── RIGHT: player panel ───────────────────────────────────────────────────
    auto *playerPanel = new QWidget;
    playerPanel->setStyleSheet("background: #0f0f0f;");
    auto *pl = new QVBoxLayout(playerPanel);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);

    // Player stack
    m_playerStack = new QStackedWidget(playerPanel);
    m_playerStack->setMinimumHeight(300);
    m_playerStack->setStyleSheet("background: #000;");

    auto *placeholder = new QWidget(m_playerStack);
    placeholder->setStyleSheet("background: #0a0a0a;");
    auto *phLayout = new QVBoxLayout(placeholder);
    phLayout->setAlignment(Qt::AlignCenter);
    auto *phIcon = new QLabel("▶");
    phIcon->setAlignment(Qt::AlignCenter);
    phIcon->setStyleSheet(
        "color: #1e1e1e; font-size: 64px; background: transparent;");
    auto *phText = new QLabel("double-click a video to play");
    phText->setAlignment(Qt::AlignCenter);
    phText->setStyleSheet(
        "color: #333; font-size: 12px; letter-spacing: 1px;"
        "font-family: 'Segoe UI', sans-serif; background: transparent;");
    phLayout->addWidget(phIcon);
    phLayout->addWidget(phText);

    m_player = new QWebEngineView(m_playerStack);
    m_playerStack->addWidget(placeholder);
    m_playerStack->addWidget(m_player);
    m_playerStack->setCurrentIndex(0);

    // Info section below player
    auto *infoArea = new QWidget(playerPanel);
    infoArea->setStyleSheet(
        "background: #0f0f0f;"
        "border-top: 1px solid #1e1e1e;");
    auto *il = new QVBoxLayout(infoArea);
    il->setContentsMargins(16, 14, 16, 14);
    il->setSpacing(6);

    m_titleLabel = new QLabel("", infoArea);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setMaximumHeight(44);
    m_titleLabel->setStyleSheet(
        "color: #f1f1f1; font-size: 15px; font-weight: bold;"
        "font-family: 'Segoe UI', 'Noto Sans', sans-serif;"
        "background: transparent;");

    m_metaLabel = new QLabel("", infoArea);
    m_metaLabel->setStyleSheet(
        "color: #aaaaaa; font-size: 12px;"
        "font-family: 'Segoe UI', 'Noto Sans', sans-serif;"
        "background: transparent;");

    m_descView = new QTextEdit(infoArea);
    m_descView->setReadOnly(true);
    m_descView->setFixedHeight(52);
    m_descView->setPlaceholderText("description...");

    // Controls row
    auto *ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(8);

    m_streamCombo = new QComboBox(infoArea);
    m_streamCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_streamCombo->setFixedHeight(36);

    m_streamBtn   = makeBtn("▶  Stream",  "#ff4444", "transparent", infoArea);
    m_downloadBtn = makeBtn("↓  Save",    "#ffaa00", "transparent", infoArea);
    m_streamBtn->setFixedHeight(36);
    m_downloadBtn->setFixedHeight(36);
    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    connect(m_streamBtn,   &QPushButton::clicked, this, &YoutubeTab::onStream);
    connect(m_downloadBtn, &QPushButton::clicked, this, &YoutubeTab::onDownload);

    ctrlRow->addWidget(m_streamCombo, 1);
    ctrlRow->addWidget(m_streamBtn);
    ctrlRow->addWidget(m_downloadBtn);

    // Progress row
    auto *progRow = new QHBoxLayout;
    progRow->setSpacing(8);
    m_progress = new QProgressBar(infoArea);
    m_progress->setRange(0, 100);
    m_progress->setFixedHeight(6);
    m_progress->hide();
    m_progressLabel = new QLabel("", infoArea);
    m_progressLabel->setStyleSheet("color:#555; font-size:11px; background:transparent;");
    progRow->addWidget(m_progress, 1);
    progRow->addWidget(m_progressLabel);

    il->addWidget(m_titleLabel);
    il->addWidget(m_metaLabel);
    il->addWidget(m_descView);
    il->addLayout(ctrlRow);
    il->addLayout(progRow);

    pl->addWidget(m_playerStack, 1);
    pl->addWidget(infoArea);

    splitter->addWidget(m_gridScroll);
    splitter->addWidget(playerPanel);
    splitter->setSizes({480, 760});

    root->addWidget(splitter, 1);
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
}

// ═════════════════════════════════════════════════════════════════════════════
//  Search
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::doSearch(const QString &q) {
    // Clear grid
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
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
    // Clear grid
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    int col = 0, row = 0;
    const int COLS = 2;

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
        connect(card, &VideoCard::activated, this, &YoutubeTab::onCardActivated);
        m_gridLayout->addWidget(card, row, col);

        col++;
        if (col >= COLS) { col = 0; row++; }
    }

    // Push content to top
    m_gridLayout->setRowStretch(row + 1, 1);

    setStatus(QString("%1 results").arg(m_results.size()), "#00cc66");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Card activated → load stream info
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onCardActivated(int idx) {
    if (idx < 0 || idx >= m_results.size()) return;
    m_currentResult = m_results[idx];

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
    setStatus("fetching stream info...", "#ffaa00");

    runBridge(m_infoProc, {"info", m_currentResult.url},
        SLOT(onInfoOutput()), SLOT(onInfoFinished(int,QProcess::ExitStatus)));
}

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
    m_descView->setPlainText(obj["description"].toString().left(400));
    m_playerStack->setCurrentIndex(0);

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
            if (st.type == "audio")
                label = QString("[AUDIO] %1kbps %2").arg(st.bitrate).arg(st.format.toUpper());
            else if (st.type == "muxed")
                label = QString("[VIDEO+AUDIO] %1 %2 %3fps").arg(st.quality, st.format.toUpper()).arg(st.fps);
            else
                label = QString("[VIDEO ONLY] %1 %2 %3fps").arg(st.quality, st.format.toUpper()).arg(st.fps);
            m_streamCombo->addItem(label, m_streams.size()-1);
        }
    };

    parse(obj["streams"].toArray());
    parse(obj["audio"].toArray());
    parse(obj["videoOnly"].toArray());

    m_streamBtn->setEnabled(true);
    m_downloadBtn->setEnabled(true);
    setStatus(QString("ready — %1 streams").arg(m_streams.size()), "#00cc66");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stream / Download
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onStream() {
    int idx = m_streamCombo->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;
    m_player->setUrl(QUrl(m_streams[idx].url));
    m_playerStack->setCurrentIndex(1);
    setStatus("streaming: " + m_streams[idx].quality, "#00cc66");
}

void YoutubeTab::onDownload() {
    int idx = m_streamCombo->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;
    const YTStream &st = m_streams[idx];

    QString safe = m_currentResult.title;
    safe.replace(QRegularExpression("[^a-zA-Z0-9 _\\-]"), "");
    safe = safe.simplified().replace(' ', '_').left(60);
    if (safe.isEmpty()) safe = "video";

    QString ext = st.type == "audio" ? st.format : "mp4";
    ext = ext.split(' ').first().toLower();
    QString def = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                  + "/" + safe + "." + ext;

    QString outPath = QFileDialog::getSaveFileName(
        this, "Save As", def,
        QString("%1 (*.%1);;All Files (*)").arg(ext));
    if (outPath.isEmpty()) return;

    m_progress->setValue(0);
    m_progress->show();
    m_progressLabel->setText("0%");
    m_downloadBtn->setEnabled(false);
    m_dlBuf.clear();
    setStatus("downloading...", "#ffaa00");

    runBridge(m_dlProc, {"download", m_currentResult.url, st.url, outPath},
        SLOT(onDownloadOutput()), SLOT(onDownloadFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onDownloadOutput() {
    m_dlBuf += QString::fromUtf8(m_dlProc->readAllStandardOutput());
    QStringList lines = m_dlBuf.split('\n', Qt::SkipEmptyParts);
    if (!m_dlBuf.endsWith('\n') && !lines.isEmpty())
        m_dlBuf = lines.takeLast();
    else
        m_dlBuf.clear();

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
    if (code != 0) { m_progress->hide(); setStatus("download failed", "#ff4444"); }
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
        setStatus("java not found — install JDK 11+", "#ff4444");
}

void YoutubeTab::setStatus(const QString &msg, const QString &color) {
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; font-family: monospace;"
                "background: transparent;").arg(color));
}

QString YoutubeTab::formatDuration(qint64 s) const {
    if (s <= 0) return "LIVE";
    if (s >= 3600)
        return QString("%1:%2:%3")
            .arg(s/3600).arg((s%3600)/60,2,10,QChar('0')).arg(s%60,2,10,QChar('0'));
    return QString("%1:%2").arg(s/60).arg(s%60,2,10,QChar('0'));
}

QString YoutubeTab::formatViews(qint64 v) const {
    if (v >= 1'000'000'000) return QString::number(v/1'000'000'000.0,'f',1)+"B";
    if (v >= 1'000'000)     return QString::number(v/1'000'000.0,'f',1)+"M";
    if (v >= 1'000)         return QString::number(v/1'000.0,'f',0)+"K";
    return QString::number(v);
}