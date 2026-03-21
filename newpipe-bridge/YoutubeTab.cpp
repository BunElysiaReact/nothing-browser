#include "YoutubeTab.h"
#include <QSplitter>
#include <QScrollArea>
#include <QJsonDocument>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>

// ─────────────────────────────────────────────────────────────────────────────
//  Style
// ─────────────────────────────────────────────────────────────────────────────
QString YoutubeTab::style() {
    return R"(
        QWidget     { background:#0d0d0d; color:#cccccc; }
        QLineEdit   {
            background:#111; color:#ddd; border:1px solid #2a2a2a;
            border-radius:3px; padding:5px 10px;
            font-family:monospace; font-size:12px;
        }
        QLineEdit:focus { border-color:#ff4444; }
        QListWidget {
            background:#0a0a0a; border:none;
            font-family:monospace; font-size:11px;
        }
        QListWidget::item {
            padding:6px 10px; border-bottom:1px solid #141414;
            color:#aaaaaa;
        }
        QListWidget::item:selected { background:#1a0000; color:#ff4444; }
        QListWidget::item:hover    { background:#111111; }
        QComboBox {
            background:#111; color:#aaa; border:1px solid #2a2a2a;
            border-radius:3px; padding:4px 8px;
            font-family:monospace; font-size:11px;
        }
        QComboBox QAbstractItemView {
            background:#111; color:#ccc;
            selection-background-color:#1a0000;
        }
        QComboBox::drop-down { border:none; }
        QTextEdit {
            background:#080808; color:#888; border:none;
            font-family:monospace; font-size:11px;
        }
        QProgressBar {
            background:#111; border:1px solid #1e1e1e;
            border-radius:2px; text-align:center; height:14px;
            font-family:monospace; font-size:10px; color:#888;
        }
        QProgressBar::chunk { background:#ff4444; border-radius:2px; }
        QScrollBar:vertical { background:#090909; width:5px; border:none; }
        QScrollBar::handle:vertical { background:#1e1e1e; border-radius:2px; min-height:16px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
    )";
}

QPushButton *YoutubeTab::btn(const QString &label, const QString &color, QWidget *p) {
    auto *b = new QPushButton(label, p);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(R"(
        QPushButton {
            background:#0d0d0d; color:%1;
            border:1px solid %1; border-radius:3px;
            font-family:monospace; font-size:11px; font-weight:bold;
            padding:5px 14px;
        }
        QPushButton:hover    { background:%2; }
        QPushButton:pressed  { background:%3; }
        QPushButton:disabled { color:#333; border-color:#222; }
    )").arg(color)
       .arg(color == "#ff4444" ? "#1a0000" : "#161616")
       .arg(color == "#ff4444" ? "#2a0000" : "#222"));
    return b;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
YoutubeTab::YoutubeTab(QWidget *parent) : QWidget(parent) {
    setStyleSheet(style());
    buildUI();
}

void YoutubeTab::buildUI() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── Top bar ───────────────────────────────────────────────────────────────
    auto *topBar = new QWidget(this);
    topBar->setFixedHeight(42);
    topBar->setStyleSheet("background:#090909; border-bottom:1px solid #1a1a1a;");
    auto *tbl = new QHBoxLayout(topBar);
    tbl->setContentsMargins(10,5,10,5); tbl->setSpacing(6);

    auto *ytLabel = new QLabel("▶  YOUTUBE", topBar);
    ytLabel->setStyleSheet(
        "color:#ff4444; font-family:monospace; font-size:13px; font-weight:bold; "
        "letter-spacing:1px;");

    m_searchBar = new QLineEdit(topBar);
    m_searchBar->setPlaceholderText("search youtube...");
    connect(m_searchBar, &QLineEdit::returnPressed, this, &YoutubeTab::onSearch);

    m_searchBtn = btn("SEARCH", "#ff4444", topBar);
    connect(m_searchBtn, &QPushButton::clicked, this, &YoutubeTab::onSearch);

    m_statusLabel = new QLabel("", topBar);
    m_statusLabel->setStyleSheet("color:#444; font-family:monospace; font-size:11px;");

    tbl->addWidget(ytLabel);
    tbl->addSpacing(8);
    tbl->addWidget(m_searchBar, 1);
    tbl->addWidget(m_searchBtn);
    tbl->addWidget(m_statusLabel);

    root->addWidget(topBar);

    // ── Main splitter: results | detail+player ────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter::handle { background:#1a1a1a; width:1px; }");

    // ── Left: results list ────────────────────────────────────────────────────
    auto *leftPanel = new QWidget(splitter);
    leftPanel->setStyleSheet("background:#0a0a0a;");
    auto *ll = new QVBoxLayout(leftPanel);
    ll->setContentsMargins(0,0,0,0); ll->setSpacing(0);

    auto *resultsHeader = new QLabel("  RESULTS", leftPanel);
    resultsHeader->setFixedHeight(24);
    resultsHeader->setStyleSheet(
        "color:#333; font-family:monospace; font-size:10px; letter-spacing:1px; "
        "background:#090909; border-bottom:1px solid #141414; padding-left:10px;");

    m_resultsList = new QListWidget(leftPanel);
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_resultsList, &QListWidget::itemDoubleClicked,
            this, &YoutubeTab::onResultSelected);

    ll->addWidget(resultsHeader);
    ll->addWidget(m_resultsList, 1);

    // ── Right: detail + player ────────────────────────────────────────────────
    m_detailPanel = new QWidget(splitter);
    auto *dl = new QVBoxLayout(m_detailPanel);
    dl->setContentsMargins(0,0,0,0); dl->setSpacing(0);

    // Player area
    m_playerStack = new QStackedWidget(m_detailPanel);

    // Placeholder
    auto *placeholder = new QLabel("Double-click a result to load it", m_playerStack);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(
        "color:#222; font-family:monospace; font-size:13px; background:#050505;");
    m_playerStack->addWidget(placeholder);

    // WebEngine player
    m_player = new QWebEngineView(m_playerStack);
    m_player->setStyleSheet("background:#000;");
    m_playerStack->addWidget(m_player);
    m_playerStack->setCurrentIndex(0);

    m_playerStack->setMinimumHeight(280);

    // ── Video info ────────────────────────────────────────────────────────────
    auto *infoWidget = new QWidget(m_detailPanel);
    infoWidget->setStyleSheet("background:#0d0d0d; border-top:1px solid #1a1a1a;");
    auto *il = new QVBoxLayout(infoWidget);
    il->setContentsMargins(14,10,14,10); il->setSpacing(6);

    m_titleLabel = new QLabel("", infoWidget);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(
        "color:#dddddd; font-family:monospace; font-size:13px; font-weight:bold; "
        "background:transparent;");

    m_metaLabel = new QLabel("", infoWidget);
    m_metaLabel->setStyleSheet(
        "color:#444; font-family:monospace; font-size:11px; background:transparent;");

    m_descView = new QTextEdit(infoWidget);
    m_descView->setReadOnly(true);
    m_descView->setFixedHeight(60);
    m_descView->setPlaceholderText("description...");

    // Stream controls
    auto *ctrlRow = new QWidget(infoWidget);
    ctrlRow->setStyleSheet("background:transparent;");
    auto *crl = new QHBoxLayout(ctrlRow);
    crl->setContentsMargins(0,0,0,0); crl->setSpacing(8);

    m_streamCombo = new QComboBox(ctrlRow);
    m_streamCombo->setMinimumWidth(200);
    m_streamCombo->setPlaceholderText("select quality...");

    m_streamBtn   = btn("▶  STREAM",   "#ff4444", ctrlRow);
    m_downloadBtn = btn("↓  DOWNLOAD", "#ffaa00", ctrlRow);

    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);

    connect(m_streamBtn,   &QPushButton::clicked, this, &YoutubeTab::onStream);
    connect(m_downloadBtn, &QPushButton::clicked, this, &YoutubeTab::onDownload);

    crl->addWidget(m_streamCombo, 1);
    crl->addWidget(m_streamBtn);
    crl->addWidget(m_downloadBtn);

    // Progress
    auto *progRow = new QWidget(infoWidget);
    progRow->setStyleSheet("background:transparent;");
    auto *prl = new QHBoxLayout(progRow);
    prl->setContentsMargins(0,0,0,0); prl->setSpacing(6);

    m_progress      = new QProgressBar(progRow);
    m_progress->setRange(0, 100);
    m_progress->hide();
    m_progressLabel = new QLabel("", progRow);
    m_progressLabel->setStyleSheet("color:#444; font-family:monospace; font-size:10px;");

    prl->addWidget(m_progress, 1);
    prl->addWidget(m_progressLabel);

    il->addWidget(m_titleLabel);
    il->addWidget(m_metaLabel);
    il->addWidget(m_descView);
    il->addWidget(ctrlRow);
    il->addWidget(progRow);

    dl->addWidget(m_playerStack, 1);
    dl->addWidget(infoWidget);

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_detailPanel);
    splitter->setSizes({320, 800});

    root->addWidget(splitter, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bridge runner
// ─────────────────────────────────────────────────────────────────────────────
QString YoutubeTab::jarPath() const {
    // Look next to the binary, then next to cwd
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/newpipe-bridge.jar",
        QDir::currentPath() + "/newpipe-bridge/build/libs/newpipe-bridge-1.0.0.jar",
        QDir::currentPath() + "/newpipe-bridge.jar",
    };
    for (auto &c : candidates)
        if (QFile::exists(c)) return c;
    return candidates.first(); // will fail gracefully
}

void YoutubeTab::runBridge(QProcess *&proc, const QStringList &args,
                            const char *readySlot, const char *finishedSlot) {
    if (proc && proc->state() != QProcess::NotRunning) {
        proc->kill();
        proc->waitForFinished(1000);
    }
    delete proc;
    proc = new QProcess(this);

    QStringList javaArgs = { "-jar", jarPath() };
    javaArgs << args;

    connect(proc, SIGNAL(readyReadStandardOutput()), this, readySlot);
    connect(proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, finishedSlot);

    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->start("java", javaArgs);

    if (!proc->waitForStarted(5000)) {
        setStatus("java not found — install JDK 11+", "#ff4444");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Search
// ─────────────────────────────────────────────────────────────────────────────
void YoutubeTab::onSearch() {
    QString q = m_searchBar->text().trimmed();
    if (q.isEmpty()) return;

    m_resultsList->clear();
    m_results.clear();
    m_searchBuf.clear();
    setStatus("searching...", "#ff4444");
    m_searchBtn->setEnabled(false);

    runBridge(m_searchProc, {"search", q},
        SLOT(onSearchOutput()), SLOT(onSearchFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onSearchOutput() {
    m_searchBuf += QString::fromUtf8(m_searchProc->readAllStandardOutput());
}

void YoutubeTab::onSearchFinished(int code, QProcess::ExitStatus) {
    m_searchBtn->setEnabled(true);
    if (code != 0) {
        setStatus("search failed — check JAR path", "#ff4444");
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(m_searchBuf.toUtf8());
    if (!doc.isArray()) {
        setStatus("bad response from bridge", "#ff4444");
        return;
    }
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

        QString label = QString("%1\n  %2  ·  %3  ·  %4 views")
            .arg(r.title)
            .arg(r.uploader)
            .arg(formatDuration(r.duration))
            .arg(formatViews(r.views));

        auto *item = new QListWidgetItem(label, m_resultsList);
        item->setData(Qt::UserRole, m_results.size() - 1);
        m_resultsList->addItem(item);
    }

    setStatus(QString("%1 results").arg(m_results.size()), "#00cc66");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load info
// ─────────────────────────────────────────────────────────────────────────────
void YoutubeTab::onResultSelected(QListWidgetItem *item) {
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_results.size()) return;

    m_currentResult = m_results[idx];
    m_titleLabel->setText(m_currentResult.title);
    m_metaLabel->setText(m_currentResult.uploader + "  ·  "
        + formatDuration(m_currentResult.duration) + "  ·  "
        + formatViews(m_currentResult.views) + " views");
    m_streamCombo->clear();
    m_streamCombo->addItem("Loading stream info...");
    m_streamBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_infoBuf.clear();
    setStatus("fetching stream info...", "#ff4444");

    runBridge(m_infoProc, {"info", m_currentResult.url},
        SLOT(onInfoOutput()), SLOT(onInfoFinished(int,QProcess::ExitStatus)));
}

void YoutubeTab::onLoadInfo() { /* triggered by double-click via onResultSelected */ }

void YoutubeTab::onInfoOutput() {
    m_infoBuf += QString::fromUtf8(m_infoProc->readAllStandardOutput());
}

void YoutubeTab::onInfoFinished(int code, QProcess::ExitStatus) {
    if (code != 0) {
        setStatus("failed to get stream info", "#ff4444");
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(m_infoBuf.toUtf8());
    if (!doc.isObject()) {
        setStatus("bad stream info response", "#ff4444");
        return;
    }
    populateStreams(doc.object());
}

void YoutubeTab::populateStreams(const QJsonObject &obj) {
    m_streams.clear();
    m_streamCombo->clear();

    m_descView->setPlainText(
        obj["description"].toString().left(500));

    auto parseStreams = [&](const QJsonArray &arr) {
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
                label = QString("[AUDIO] %1kbps %2").arg(st.bitrate).arg(st.format);
            else
                label = QString("[%1] %2 %3 %4fps")
                    .arg(st.type == "muxed" ? "VIDEO+AUDIO" : "VIDEO-ONLY")
                    .arg(st.quality).arg(st.format).arg(st.fps);
            m_streamCombo->addItem(label, m_streams.size() - 1);
        }
    };

    parseStreams(obj["streams"].toArray());    // muxed first (streamable)
    parseStreams(obj["audio"].toArray());      // then audio
    parseStreams(obj["videoOnly"].toArray()); // video-only last

    m_streamBtn->setEnabled(true);
    m_downloadBtn->setEnabled(true);
    setStatus("ready", "#00cc66");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stream
// ─────────────────────────────────────────────────────────────────────────────
void YoutubeTab::onStream() {
    int idx = m_streamCombo->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;

    const YTStream &st = m_streams[idx];

    // Load stream URL directly into Qt WebEngine player
    // Muxed mp4 streams play natively; for webm/audio we still try
    m_player->setUrl(QUrl(st.url));
    m_playerStack->setCurrentIndex(1);
    setStatus("streaming: " + st.quality, "#00cc66");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Download
// ─────────────────────────────────────────────────────────────────────────────
void YoutubeTab::onDownload() {
    int idx = m_streamCombo->currentData().toInt();
    if (idx < 0 || idx >= m_streams.size()) return;

    const YTStream &st = m_streams[idx];

    // Suggest filename
    QString safe = m_currentResult.title;
    safe.replace(QRegularExpression("[^a-zA-Z0-9 _\\-]"), "");
    safe = safe.simplified().replace(' ', '_').left(60);
    if (safe.isEmpty()) safe = "video";

    QString ext  = st.type == "audio" ? st.format : "mp4";
    QString def  = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
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
    // Each line is a JSON object: {progress:N} or {done:true,...}
    m_dlBuf += QString::fromUtf8(m_dlProc->readAllStandardOutput());
    QStringList lines = m_dlBuf.split('\n', Qt::SkipEmptyParts);
    // Keep incomplete last line in buffer
    if (!m_dlBuf.endsWith('\n') && !lines.isEmpty())
        m_dlBuf = lines.takeLast();
    else
        m_dlBuf.clear();

    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
        if (!doc.isObject()) continue;
        QJsonObject o = doc.object();
        if (o.contains("progress")) {
            int pct = o["progress"].toInt();
            m_progress->setValue(pct);
            m_progressLabel->setText(QString("%1%").arg(pct));
        }
    }
}

void YoutubeTab::onDownloadFinished(int code, QProcess::ExitStatus) {
    m_downloadBtn->setEnabled(true);

    // Parse last line for done/error
    QStringList lines = m_dlBuf.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.trimmed().toUtf8());
        if (!doc.isObject()) continue;
        QJsonObject o = doc.object();
        if (o["done"].toBool()) {
            m_progress->setValue(100);
            m_progressLabel->setText("done");
            setStatus("saved: " + o["path"].toString(), "#00cc66");
            return;
        }
    }

    if (code != 0) {
        m_progress->hide();
        setStatus("download failed", "#ff4444");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
void YoutubeTab::setStatus(const QString &msg, const QString &color) {
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("color:%1; font-family:monospace; font-size:11px;").arg(color));
}

QString YoutubeTab::formatDuration(qint64 s) const {
    if (s <= 0) return "LIVE";
    return QString("%1:%2")
        .arg(s / 60)
        .arg(s % 60, 2, 10, QChar('0'));
}

QString YoutubeTab::formatViews(qint64 v) const {
    if (v >= 1'000'000) return QString::number(v / 1'000'000) + "M";
    if (v >= 1'000)     return QString::number(v / 1'000) + "K";
    return QString::number(v);
}
