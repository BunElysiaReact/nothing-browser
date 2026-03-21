#include "YoutubeTab.h"
#include <QSplitter>
#include <QJsonDocument>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QPainter>

// ═════════════════════════════════════════════════════════════════════════════
//  Design tokens
//  bg:      #080a06  #0d0f0a  #111408  #171b10
//  surface: #1a1f12  #222b16  #2a361a
//  green:   #3d6b27  #5a9e38  #7ec84a  #a8e070
//  amber:   #9a7420  #c49a2c  #e8c060
//  rust:    #7a3c1e  #a85530  #cc7a48
//  text:    #cce0b0  #8aaa6a  #4a6030  #2a3818
//  border:  #1e2812  #243018  #2e3c1e
// ═════════════════════════════════════════════════════════════════════════════

QString YoutubeTab::globalStyle() {
    return R"(
        QWidget {
            background: #080a06;
            color: #cce0b0;
            font-family: "JetBrains Mono", "Fira Code", "Cascadia Code", monospace;
        }

        QSplitter::handle { background: #1e2812; width: 1px; height: 1px; }

        /* ── Inputs ─────────────────────────────────────────────────────── */
        QLineEdit {
            background: #0d0f0a;
            color: #cce0b0;
            border: 1px solid #2a3818;
            border-radius: 2px;
            padding: 6px 12px;
            font-size: 11px;
            selection-background-color: #2a3818;
        }
        QLineEdit:focus {
            border-color: #5a9e38;
            background: #0f120a;
        }
        QLineEdit::placeholder { color: #2e3c1e; }

        /* ── List ────────────────────────────────────────────────────────── */
        QListWidget {
            background: #080a06;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 10px 14px;
            border-bottom: 1px solid #111408;
            color: #8aaa6a;
            min-height: 52px;
        }
        QListWidget::item:selected {
            background: #0d150a;
            color: #cce0b0;
            border-left: 3px solid #7ec84a;
        }
        QListWidget::item:hover:!selected {
            background: #0d0f0a;
            color: #a8c880;
        }

        /* ── Combo ───────────────────────────────────────────────────────── */
        QComboBox {
            background: #111408;
            color: #cce0b0;
            border: 1px solid #2a3818;
            border-radius: 2px;
            padding: 5px 10px;
            font-size: 10px;
        }
        QComboBox:focus { border-color: #5a9e38; }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox::down-arrow {
            width: 8px; height: 8px;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #4a6030;
        }
        QComboBox QAbstractItemView {
            background: #0d0f0a;
            color: #cce0b0;
            border: 1px solid #2a3818;
            selection-background-color: #1a2510;
            outline: none;
        }

        /* ── TextEdit ────────────────────────────────────────────────────── */
        QTextEdit {
            background: #080a06;
            color: #4a6030;
            border: none;
            font-size: 10px;
            padding: 10px 14px;
        }

        /* ── Scrollbars ──────────────────────────────────────────────────── */
        QScrollBar:vertical {
            background: #080a06; width: 3px; border: none; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #2a3818; border-radius: 1px; min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: #3d5525; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

        QScrollBar:horizontal {
            background: #080a06; height: 3px; border: none;
        }
        QScrollBar::handle:horizontal {
            background: #2a3818; border-radius: 1px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        /* ── Labels ──────────────────────────────────────────────────────── */
        QLabel { background: transparent; }

        /* ── ToolTip ─────────────────────────────────────────────────────── */
        QToolTip {
            background: #0d0f0a;
            color: #cce0b0;
            border: 1px solid #2a3818;
            padding: 4px 8px;
            font-size: 10px;
        }
    )";
}

QPushButton *YoutubeTab::makeBtn(const QString &label, const QString &fg,
                                  const QString &border, const QString &hoverBg,
                                  QWidget *parent) {
    auto *b = new QPushButton(label, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(R"(
        QPushButton {
            background: transparent;
            color: %1;
            border: 1px solid %2;
            border-radius: 2px;
            font-family: "JetBrains Mono", "Fira Code", monospace;
            font-size: 10px;
            font-weight: bold;
            padding: 6px 16px;
            letter-spacing: 1.5px;
        }
        QPushButton:hover   { background: %3; border-color: %1; }
        QPushButton:pressed { background: %3; }
        QPushButton:disabled {
            color: #2a3818;
            border-color: #1a2010;
        }
    )").arg(fg, border, hoverBg));
    return b;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════
YoutubeTab::YoutubeTab(QWidget *parent) : QWidget(parent) {
    setStyleSheet(globalStyle());
    buildUI();
}

void YoutubeTab::buildUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    auto *topBar = new QWidget(this);
    topBar->setFixedHeight(48);
    topBar->setStyleSheet(
        "background: #0d0f0a;"
        "border-bottom: 1px solid #1e2812;");
    auto *tbl = new QHBoxLayout(topBar);
    tbl->setContentsMargins(16, 0, 16, 0);
    tbl->setSpacing(12);

    // Logo
    auto *logoWrap = new QWidget(topBar);
    logoWrap->setStyleSheet("background: transparent;");
    logoWrap->setFixedWidth(110);
    auto *logoLayout = new QHBoxLayout(logoWrap);
    logoLayout->setContentsMargins(0, 0, 0, 0);
    logoLayout->setSpacing(0);
    auto *logo = new QLabel(logoWrap);
    logo->setText(
        "<span style='color:#5a9e38; font-size:14px; font-weight:900; letter-spacing:3px;'>NTH</span>"
        "<span style='color:#c49a2c; font-size:14px; font-weight:900; letter-spacing:3px;'>TUBE</span>");
    logo->setTextFormat(Qt::RichText);
    logo->setStyleSheet("background: transparent;");
    logoLayout->addWidget(logo);

    // Separator line accent
    auto *logoAccent = new QWidget(topBar);
    logoAccent->setFixedSize(1, 24);
    logoAccent->setStyleSheet("background: #1e2812;");

    m_searchInput = new QLineEdit(topBar);
    m_searchInput->setPlaceholderText("search for a video...");
    m_searchInput->setMinimumHeight(32);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &YoutubeTab::onSearch);

    m_searchBtn = makeBtn("SEARCH", "#7ec84a", "#3d6b27", "#0d1a06", topBar);
    m_searchBtn->setFixedHeight(32);
    connect(m_searchBtn, &QPushButton::clicked, this, &YoutubeTab::onSearch);

    // Status dot
    m_statusDot = new QLabel(topBar);
    m_statusDot->setFixedSize(6, 6);
    m_statusDot->setStyleSheet("background: #5a9e38; border-radius: 3px;");

    tbl->addWidget(logoWrap);
    tbl->addWidget(logoAccent);
    tbl->addSpacing(4);
    tbl->addWidget(m_searchInput, 1);
    tbl->addWidget(m_searchBtn);
    tbl->addWidget(m_statusDot);

    root->addWidget(topBar);

    // ── Main splitter ─────────────────────────────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background: #1e2812; }");

    // ════════════════════════════════════════════════════════════════════════
    //  LEFT — results panel
    // ════════════════════════════════════════════════════════════════════════
    auto *leftWrap = new QWidget(splitter);
    leftWrap->setStyleSheet("background: #080a06;");
    auto *lv = new QVBoxLayout(leftWrap);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);

    // Results header
    auto *resultsHdr = new QWidget(leftWrap);
    resultsHdr->setFixedHeight(32);
    resultsHdr->setStyleSheet(
        "background: #0d0f0a;"
        "border-bottom: 1px solid #1e2812;");
    auto *rhl = new QHBoxLayout(resultsHdr);
    rhl->setContentsMargins(14, 0, 14, 0);
    rhl->setSpacing(8);

    auto *rLabel = new QLabel("RESULTS", resultsHdr);
    rLabel->setStyleSheet(
        "color: #2e3c1e; font-size: 9px; font-weight: bold;"
        "letter-spacing: 2px; background: transparent;");

    m_countBadge = new QLabel("", resultsHdr);
    m_countBadge->setStyleSheet(
        "color: #c49a2c; font-size: 9px;"
        "background: #1a1408; border: 1px solid #3a2e10;"
        "border-radius: 2px; padding: 0px 5px;");
    m_countBadge->hide();

    rhl->addWidget(rLabel);
    rhl->addStretch();
    rhl->addWidget(m_countBadge);

    m_resultsList = new QListWidget(leftWrap);
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsList->setUniformItemSizes(false);
    m_resultsList->setSpacing(0);
    connect(m_resultsList, &QListWidget::itemClicked,
            this, &YoutubeTab::onResultClicked);
    connect(m_resultsList, &QListWidget::itemDoubleClicked,
            this, &YoutubeTab::onResultDoubleClicked);

    lv->addWidget(resultsHdr);
    lv->addWidget(m_resultsList, 1);

    // ════════════════════════════════════════════════════════════════════════
    //  RIGHT — player + info + controls + description
    // ════════════════════════════════════════════════════════════════════════
    auto *rightWrap = new QWidget(splitter);
    rightWrap->setStyleSheet("background: #080a06;");
    auto *rv = new QVBoxLayout(rightWrap);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);

    // Player stack
    m_playerStack = new QStackedWidget(rightWrap);
    m_playerStack->setMinimumHeight(280);
    m_playerStack->setStyleSheet("background: #040604;");

    // Placeholder
    m_playerPlaceholder = new QWidget(m_playerStack);
    m_playerPlaceholder->setStyleSheet("background: #040604;");
    auto *phl = new QVBoxLayout(m_playerPlaceholder);
    phl->setAlignment(Qt::AlignCenter);
    phl->setSpacing(10);

    // Big play icon
    auto *playIcon = new QLabel("▶", m_playerPlaceholder);
    playIcon->setAlignment(Qt::AlignCenter);
    playIcon->setStyleSheet(
        "color: #1a2812; font-size: 48px; background: transparent;");

    auto *phLine1 = new QLabel("double-click a result to load", m_playerPlaceholder);
    phLine1->setAlignment(Qt::AlignCenter);
    phLine1->setStyleSheet(
        "color: #1e2c14; font-size: 10px; letter-spacing: 1px;"
        "background: transparent;");

    phl->addWidget(playIcon);
    phl->addWidget(phLine1);

    // Actual player
    m_player = new QWebEngineView(m_playerStack);
    m_player->setStyleSheet("background: #000;");

    m_playerStack->addWidget(m_playerPlaceholder);
    m_playerStack->addWidget(m_player);
    m_playerStack->setCurrentIndex(0);

    // Progress scrubber — thicker, more visible
    m_scrubberBar = new QWidget(rightWrap);
    m_scrubberBar->setFixedHeight(4);
    m_scrubberBar->setStyleSheet("background: #111408;");
    auto *scrubLayout = new QHBoxLayout(m_scrubberBar);
    scrubLayout->setContentsMargins(0, 0, 0, 0);
    scrubLayout->setSpacing(0);
    m_scrubberFill = new QWidget(m_scrubberBar);
    m_scrubberFill->setFixedWidth(0);
    m_scrubberFill->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #3d6b27, stop:1 #7ec84a);");
    scrubLayout->addWidget(m_scrubberFill);
    scrubLayout->addStretch();

    // Info bar — taller, more spacious
    auto *infoBar = new QWidget(rightWrap);
    infoBar->setFixedHeight(66);
    infoBar->setStyleSheet(
        "background: #0d0f0a;"
        "border-top: 1px solid #1e2812;"
        "border-bottom: 1px solid #1e2812;");
    auto *il = new QVBoxLayout(infoBar);
    il->setContentsMargins(16, 8, 16, 8);
    il->setSpacing(5);

    m_videoTitle = new QLabel("Select a video", infoBar);
    m_videoTitle->setStyleSheet(
        "color: #cce0b0; font-size: 13px; font-weight: bold;"
        "letter-spacing: 0.3px; background: transparent;");
    m_videoTitle->setWordWrap(false);

    auto *metaRow = new QWidget(infoBar);
    metaRow->setStyleSheet("background: transparent;");
    auto *ml = new QHBoxLayout(metaRow);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);

    // Uploader with colored dot prefix
    auto *uploaderDot = new QLabel("◆ ", metaRow);
    uploaderDot->setStyleSheet(
        "color: #a85530; font-size: 8px; background: transparent;");
    m_metaUploader = new QLabel("—", metaRow);
    m_metaUploader->setStyleSheet(
        "color: #8a6040; font-size: 10px; background: transparent;");

    auto *sep1 = new QLabel("  ·  ", metaRow);
    sep1->setStyleSheet("color: #2a3818; background: transparent;");

    auto *durationDot = new QLabel("◆ ", metaRow);
    durationDot->setStyleSheet(
        "color: #c49a2c; font-size: 8px; background: transparent;");
    m_metaDuration = new QLabel("—", metaRow);
    m_metaDuration->setStyleSheet(
        "color: #9a7820; font-size: 10px; background: transparent;");

    auto *sep2 = new QLabel("  ·  ", metaRow);
    sep2->setStyleSheet("color: #2a3818; background: transparent;");

    auto *viewsDot = new QLabel("◆ ", metaRow);
    viewsDot->setStyleSheet(
        "color: #5a9e38; font-size: 8px; background: transparent;");
    m_metaViews = new QLabel("—", metaRow);
    m_metaViews->setStyleSheet(
        "color: #4a7c2f; font-size: 10px; background: transparent;");

    ml->addWidget(uploaderDot);
    ml->addWidget(m_metaUploader);
    ml->addWidget(sep1);
    ml->addWidget(durationDot);
    ml->addWidget(m_metaDuration);
    ml->addWidget(sep2);
    ml->addWidget(viewsDot);
    ml->addWidget(m_metaViews);
    ml->addStretch();

    il->addWidget(m_videoTitle);
    il->addWidget(metaRow);

    // Controls bar
    auto *ctrlBar = new QWidget(rightWrap);
    ctrlBar->setFixedHeight(46);
    ctrlBar->setStyleSheet(
        "background: #0a0c08;"
        "border-bottom: 1px solid #1e2812;");
    auto *cl = new QHBoxLayout(ctrlBar);
    cl->setContentsMargins(16, 6, 16, 6);
    cl->setSpacing(10);

    m_qualitySelect = new QComboBox(ctrlBar);
    m_qualitySelect->setMinimumWidth(220);
    m_qualitySelect->setFixedHeight(30);
    m_qualitySelect->addItem("— load a video first —");

    m_streamBtn   = makeBtn("▶  STREAM",   "#7ec84a", "#3d6b27", "#0d1a08", ctrlBar);
    m_downloadBtn = makeBtn("↓  DOWNLOAD", "#e8c060", "#9a7420", "#1a1400", ctrlBar);
    m_loopBtn     = makeBtn("⟳  LOOP",     "#8aaa6a", "#2a3818", "#0d150a", ctrlBar);
    m_streamBtn->setFixedHeight(30);
    m_downloadBtn->setFixedHeight(30);
    m_loopBtn->setFixedHeight(30);
    m_loopBtn->setCheckable(true);
    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    connect(m_streamBtn,   &QPushButton::clicked,  this, &YoutubeTab::onStream);
    connect(m_downloadBtn, &QPushButton::clicked,  this, &YoutubeTab::onDownload);
    connect(m_loopBtn,     &QPushButton::toggled,  this, &YoutubeTab::onLoopToggled);

    // Progress widget
    m_dlProgressWrap = new QWidget(ctrlBar);
    m_dlProgressWrap->hide();
    m_dlProgressWrap->setStyleSheet("background: transparent;");
    auto *pl = new QHBoxLayout(m_dlProgressWrap);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(8);

    auto *progBarOuter = new QWidget(m_dlProgressWrap);
    progBarOuter->setFixedHeight(3);
    progBarOuter->setMinimumWidth(100);
    progBarOuter->setStyleSheet("background: #1a1a10; border-radius: 1px;");
    auto *progBarInner = new QHBoxLayout(progBarOuter);
    progBarInner->setContentsMargins(0, 0, 0, 0);
    progBarInner->setSpacing(0);
    m_progressFill = new QWidget(progBarOuter);
    m_progressFill->setFixedWidth(0);
    m_progressFill->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #9a7420, stop:1 #e8c060); border-radius: 1px;");
    progBarInner->addWidget(m_progressFill);
    progBarInner->addStretch();

    m_progressLabel = new QLabel("0%", m_dlProgressWrap);
    m_progressLabel->setStyleSheet(
        "color: #c49a2c; font-size: 9px; letter-spacing: 1px;"
        "background: transparent;");

    pl->addWidget(progBarOuter, 1);
    pl->addWidget(m_progressLabel);

    cl->addWidget(m_qualitySelect, 1);
    cl->addWidget(m_streamBtn);
    cl->addWidget(m_downloadBtn);
    cl->addWidget(m_loopBtn);
    cl->addWidget(m_dlProgressWrap);

    // Description header
    auto *descHdr = new QWidget(rightWrap);
    descHdr->setFixedHeight(26);
    descHdr->setStyleSheet(
        "background: #0a0c08;"
        "border-bottom: 1px solid #111408;");
    auto *dhl = new QHBoxLayout(descHdr);
    dhl->setContentsMargins(14, 0, 14, 0);
    auto *descLabel = new QLabel("DESCRIPTION", descHdr);
    descLabel->setStyleSheet(
        "color: #2e3c1e; font-size: 9px; font-weight: bold;"
        "letter-spacing: 2px; background: transparent;");
    dhl->addWidget(descLabel);

    // Description text
    m_descText = new QTextEdit(rightWrap);
    m_descText->setReadOnly(true);
    m_descText->setPlaceholderText("// description will appear here");
    m_descText->setStyleSheet(
        "QTextEdit {"
        "  background: #080a06;"
        "  color: #3d5525;"
        "  border: none;"
        "  font-size: 10px;"
        "  padding: 12px 16px;"
        "}");

    rv->addWidget(m_playerStack, 3);
    rv->addWidget(m_scrubberBar);
    rv->addWidget(infoBar);
    rv->addWidget(ctrlBar);
    rv->addWidget(descHdr);
    rv->addWidget(m_descText, 1);

    splitter->addWidget(leftWrap);
    splitter->addWidget(rightWrap);
    splitter->setSizes({320, 900});

    root->addWidget(splitter, 1);

    // ── Status bar ────────────────────────────────────────────────────────────
    auto *statusBar = new QWidget(this);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet(
        "background: #0a0c08;"
        "border-top: 1px solid #1a2010;");
    auto *sl = new QHBoxLayout(statusBar);
    sl->setContentsMargins(14, 0, 14, 0);
    sl->setSpacing(8);

    // Left accent line
    auto *statusAccent = new QWidget(statusBar);
    statusAccent->setFixedSize(2, 10);
    statusAccent->setStyleSheet("background: #3d6b27; border-radius: 1px;");

    m_statusMsg = new QLabel("BRIDGE READY", statusBar);
    m_statusMsg->setStyleSheet(
        "color: #5a9e38; font-size: 9px; font-weight: bold;"
        "letter-spacing: 1px; background: transparent;");

    sl->addWidget(statusAccent);
    sl->addWidget(m_statusMsg);
    sl->addStretch();

    root->addWidget(statusBar);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Helpers
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
        setStatus("java not found — install JDK 11+", "#cc4a4a");
}

void YoutubeTab::setStatus(const QString &msg, const QString &color) {
    m_statusMsg->setText(msg.toUpper());
    m_statusMsg->setStyleSheet(
        QString("color: %1; font-size: 9px; font-weight: bold;"
                "letter-spacing: 1px; background: transparent;").arg(color));
}

void YoutubeTab::setDotState(const QString &state) {
    if (state == "active")
        m_statusDot->setStyleSheet(
            "background: #5a9e38; border-radius: 3px;");
    else if (state == "loading")
        m_statusDot->setStyleSheet(
            "background: #c49a2c; border-radius: 3px;");
    else
        m_statusDot->setStyleSheet(
            "background: #1e2812; border-radius: 3px;");
}

void YoutubeTab::setProgress(int pct) {
    if (pct < 0) { m_dlProgressWrap->hide(); return; }
    m_dlProgressWrap->show();
    int outerW = m_dlProgressWrap->width() - m_progressLabel->width() - 14;
    if (outerW > 0)
        m_progressFill->setFixedWidth(outerW * pct / 100);
    m_progressLabel->setText(QString("%1%").arg(pct));
}

QString YoutubeTab::formatDuration(qint64 s) const {
    if (s <= 0) return "LIVE";
    if (s >= 3600)
        return QString("%1:%2:%3")
            .arg(s/3600)
            .arg((s%3600)/60, 2, 10, QChar('0'))
            .arg(s%60, 2, 10, QChar('0'));
    return QString("%1:%2").arg(s/60).arg(s%60, 2, 10, QChar('0'));
}

QString YoutubeTab::formatViews(qint64 v) const {
    if (v >= 1'000'000'000) return QString::number(v/1'000'000'000.0, 'f', 1) + "B views";
    if (v >= 1'000'000)     return QString::number(v/1'000'000.0,     'f', 1) + "M views";
    if (v >= 1'000)         return QString::number(v/1'000.0,         'f', 0) + "K views";
    return QString::number(v) + " views";
}

// ═════════════════════════════════════════════════════════════════════════════
//  Search
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onSearch() {
    QString q = m_searchInput->text().trimmed();
    if (q.isEmpty()) return;
    m_resultsList->clear();
    m_results.clear();
    m_searchBuf.clear();
    m_searchBtn->setEnabled(false);
    m_countBadge->hide();
    setDotState("loading");
    setStatus("searching...", "#c49a2c");
    runBridge(m_searchProc, {"search", q},
        SLOT(onSearchOutput()), SLOT(onSearchFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onSearchOutput() {
    m_searchBuf += QString::fromUtf8(m_searchProc->readAllStandardOutput());
}

void YoutubeTab::onSearchFinished(int code, QProcess::ExitStatus) {
    m_searchBtn->setEnabled(true);
    setDotState(code == 0 ? "active" : "idle");
    if (code != 0) { setStatus("search failed", "#cc4a4a"); return; }
    QJsonDocument doc = QJsonDocument::fromJson(m_searchBuf.toUtf8());
    if (!doc.isArray()) { setStatus("bad response", "#cc4a4a"); return; }
    populateResults(doc.array());
}

void YoutubeTab::populateResults(const QJsonArray &arr) {
    m_results.clear();
    m_resultsList->clear();
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

        QString durStr = formatDuration(r.duration);
        QString viewStr = formatViews(r.views);

        // Two-line display: title on line 1, meta on line 2
        QString display = r.title
            + "\n"
            + r.uploader
            + "   " + durStr
            + "   " + viewStr;

        auto *item = new QListWidgetItem(display, m_resultsList);
        item->setData(Qt::UserRole, m_results.size() - 1);
        item->setToolTip(r.title);
        item->setSizeHint(QSize(0, 54));
    }

    m_countBadge->setText(QString::number(m_results.size()));
    m_countBadge->show();
    setStatus(QString("found %1 results").arg(m_results.size()), "#5a9e38");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Result selection
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onResultClicked(QListWidgetItem *) {}

void YoutubeTab::onResultDoubleClicked(QListWidgetItem *item) {
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_results.size()) return;
    m_current = m_results[idx];

    // Update info bar immediately
    m_videoTitle->setText(m_current.title);
    m_metaUploader->setText(m_current.uploader);
    m_metaDuration->setText(formatDuration(m_current.duration));
    m_metaViews->setText(formatViews(m_current.views));
    m_descText->setPlainText("// fetching stream info...");
    m_qualitySelect->clear();
    m_qualitySelect->addItem("fetching streams...");
    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_scrubberFill->setFixedWidth(0);
    m_infoBuf.clear();
    setDotState("loading");
    setStatus("fetching stream info...", "#c49a2c");
    runBridge(m_infoProc, {"info", m_current.url},
        SLOT(onInfoOutput()), SLOT(onInfoFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onInfoOutput() {
    m_infoBuf += QString::fromUtf8(m_infoProc->readAllStandardOutput());
}

void YoutubeTab::onInfoFinished(int code, QProcess::ExitStatus) {
    setDotState(code == 0 ? "active" : "idle");
    if (code != 0) { setStatus("failed to get streams", "#cc4a4a"); return; }
    QJsonDocument doc = QJsonDocument::fromJson(m_infoBuf.toUtf8());
    if (!doc.isObject()) { setStatus("bad stream response", "#cc4a4a"); return; }
    populateStreams(doc.object());
}

void YoutubeTab::populateStreams(const QJsonObject &obj) {
    m_streams.clear();
    m_qualitySelect->clear();
    m_descText->setPlainText(obj["description"].toString());
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
                label = QString("[AUDIO]  %1kbps  %2")
                    .arg(st.bitrate).arg(st.format.toUpper());
            else if (st.type == "muxed")
                label = QString("[VIDEO+AUDIO]  %1  %2  %3fps")
                    .arg(st.quality).arg(st.format.toUpper()).arg(st.fps);
            else
                label = QString("[VIDEO ONLY]  %1  %2  %3fps")
                    .arg(st.quality).arg(st.format.toUpper()).arg(st.fps);

            m_qualitySelect->addItem(label, m_streams.size() - 1);
        }
    };

    parse(obj["streams"].toArray());
    parse(obj["audio"].toArray());
    parse(obj["videoOnly"].toArray());

    m_streamBtn->setEnabled(true);
    m_downloadBtn->setEnabled(true);
    setStatus(QString("ready  —  %1 streams").arg(m_streams.size()), "#5a9e38");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stream
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onStream() {
    int idx = m_qualitySelect->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;
    const YTStream &st = m_streams[idx];
    m_player->setUrl(QUrl(st.url));
    m_playerStack->setCurrentIndex(1);
    if (m_looping) {
        connect(m_player, &QWebEngineView::loadFinished, this, [this](bool) {
            m_player->page()->runJavaScript(
                "var v=document.querySelector('video');"
                "if(v){ v.loop=true; }");
        }, Qt::SingleShotConnection);
    }
    setStatus(QString("streaming  —  %1  %2").arg(st.quality, st.format), "#5a9e38");
}

// ═════════════════════════════════════════════════════════════════════════════
//  Download
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onDownload() {
    int idx = m_qualitySelect->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;
    const YTStream &st = m_streams[idx];

    QString safe = m_current.title;
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

    setProgress(0);
    m_downloadBtn->setEnabled(false);
    m_dlBuf.clear();
    setDotState("loading");
    setStatus("downloading...", "#a85530");
    runBridge(m_dlProc, {"download", m_current.url, st.url, outPath},
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
        if (pct >= 0) setProgress(pct);
    }
}

void YoutubeTab::onDownloadFinished(int code, QProcess::ExitStatus) {
    m_downloadBtn->setEnabled(true);
    setDotState(code == 0 ? "active" : "idle");
    QStringList lines = m_dlBuf.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
        if (!doc.isObject()) continue;
        if (doc.object()["done"].toBool()) {
            setProgress(100);
            setStatus("saved  —  " + doc.object()["path"].toString(), "#5a9e38");
            return;
        }
    }
    if (code != 0) { setProgress(-1); setStatus("download failed", "#cc4a4a"); }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Loop toggle
// ═════════════════════════════════════════════════════════════════════════════
void YoutubeTab::onLoopToggled(bool checked) {
    m_looping = checked;
    if (checked) {
        m_loopBtn->setText("⟳  LOOP ON");
        m_loopBtn->setStyleSheet(QString(R"(
            QPushButton {
                background: #0d2010; color: #a8e070;
                border: 1px solid #3d6b27; border-radius: 2px;
                font-family: "JetBrains Mono", "Fira Code", monospace;
                font-size: 10px; font-weight: bold;
                padding: 6px 16px; letter-spacing: 1.5px;
            }
            QPushButton:hover   { background: #162a10; }
            QPushButton:pressed { background: #162a10; }
        )"));
        if (m_player && m_playerStack->currentIndex() == 1)
            m_player->page()->runJavaScript(
                "var v=document.querySelector('video');"
                "if(v){ v.loop=true; }");
        setStatus("loop on", "#a8e070");
    } else {
        m_loopBtn->setText("⟳  LOOP");
        m_loopBtn->setStyleSheet(QString(R"(
            QPushButton {
                background: transparent; color: #8aaa6a;
                border: 1px solid #2a3818; border-radius: 2px;
                font-family: "JetBrains Mono", "Fira Code", monospace;
                font-size: 10px; font-weight: bold;
                padding: 6px 16px; letter-spacing: 1.5px;
            }
            QPushButton:hover   { background: #0d150a; }
            QPushButton:pressed { background: #0d150a; }
        )"));
        if (m_player && m_playerStack->currentIndex() == 1)
            m_player->page()->runJavaScript(
                "var v=document.querySelector('video');"
                "if(v){ v.loop=false; }");
        setStatus("loop off", "#4a6030");
    }
}