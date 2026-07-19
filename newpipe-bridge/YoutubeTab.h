#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QComboBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWebEngineView>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileDialog>
#include <QStandardPaths>

// ── One search result ─────────────────────────────────────────────────────────
struct YTResult {
    QString id;
    QString url;
    QString title;
    QString uploader;
    QString thumbnail;
    qint64  duration = 0;
    qint64  views    = 0;
};

// ── One stream option ─────────────────────────────────────────────────────────
struct YTStream {
    QString type;     // muxed | video-only | audio
    QString quality;  // "1080p", "720p", etc.  (empty for audio)
    QString format;   // mp4, webm, m4a, etc.
    QString url;
    int     bitrate = 0;  // for audio streams
    int     fps     = 0;
};

class YoutubeTab : public QWidget {
    Q_OBJECT
public:
    explicit YoutubeTab(QWidget *parent = nullptr);

private slots:
    void onSearch();
    void onResultSelected(QListWidgetItem *item);
    void onLoadInfo();
    void onStream();
    void onDownload();

    // QProcess slots
    void onSearchOutput();
    void onSearchFinished(int code, QProcess::ExitStatus);
    void onInfoOutput();
    void onInfoFinished(int code, QProcess::ExitStatus);
    void onDownloadOutput();
    void onDownloadFinished(int code, QProcess::ExitStatus);

private:
    // ── UI ────────────────────────────────────────────────────────────────────
    QLineEdit      *m_searchBar;
    QPushButton    *m_searchBtn;
    QListWidget    *m_resultsList;
    QLabel         *m_statusLabel;

    // Video detail panel
    QWidget        *m_detailPanel;
    QLabel         *m_titleLabel;
    QLabel         *m_metaLabel;
    QTextEdit      *m_descView;
    QComboBox      *m_streamCombo;
    QPushButton    *m_streamBtn;
    QPushButton    *m_downloadBtn;
    QComboBox      *m_qualityCombo;
    QProgressBar   *m_progress;
    QLabel         *m_progressLabel;

    // Player
    QWebEngineView *m_player;
    QStackedWidget *m_playerStack; // index 0 = placeholder, 1 = player

    // ── State ─────────────────────────────────────────────────────────────────
    QList<YTResult> m_results;
    QList<YTStream> m_streams;
    YTResult        m_currentResult;
    QString         m_bridgeJar;

    // ── Processes ─────────────────────────────────────────────────────────────
    QProcess *m_searchProc  = nullptr;
    QProcess *m_infoProc    = nullptr;
    QProcess *m_dlProc      = nullptr;
    QString   m_searchBuf;
    QString   m_infoBuf;
    QString   m_dlBuf;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildUI();
    void setStatus(const QString &msg, const QString &color = "#888");
    void populateResults(const QJsonArray &arr);
    void populateStreams(const QJsonObject &obj);
    void runBridge(QProcess *&proc, const QStringList &args,
                   const char *readySlot, const char *finishedSlot);
    QString formatDuration(qint64 secs) const;
    QString formatViews(qint64 v) const;
    QString jarPath() const;

    static QString style();
    static QPushButton *btn(const QString &label, const QString &color, QWidget *p);
};
