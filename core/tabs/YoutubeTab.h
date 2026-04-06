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
#include <QGridLayout>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileDialog>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QScrollArea>
#include <QFile>
#include <QMouseEvent>
#include <QTimer>
#include <QFrame>

struct YTResult {
    QString id, url, title, uploader, thumbnail;
    qint64  duration = 0, views = 0;
};

struct YTStream {
    QString type, quality, format, url;
    int     bitrate = 0, fps = 0;
};

// ── Queue entry ───────────────────────────────────────────────────────────────
struct QueueEntry {
    YTResult result;
    QString  streamUrl;   // filled when we fetch info
    QString  quality;
    bool     ready = false;
};

// ── Video card ────────────────────────────────────────────────────────────────
class VideoCard : public QWidget {
    Q_OBJECT
public:
    explicit VideoCard(const YTResult &r, int idx,
                       QNetworkAccessManager *nam, QWidget *parent = nullptr);
    int resultIndex() const { return m_idx; }

signals:
    void activated(int idx);       // double-click → play now
    void addToQueue(int idx);      // watch next button

protected:
    void mouseDoubleClickEvent(QMouseEvent *) override { emit activated(m_idx); }
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *)      override;

private:
    int     m_idx;
    QLabel *m_thumb;
    QLabel *m_duration;
    QLabel *m_title;
    QLabel *m_channel;
    QLabel *m_views;
};

// ── Mini player bar (floats over home page while queue plays) ─────────────────
class MiniPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MiniPlayer(QWidget *parent = nullptr);
    void setNowPlaying(const QString &title, const QString &channel);
    void setQueueSize(int size, int current);

signals:
    void requestNext();
    void requestPrev();
    void requestToggleLoop();
    void requestExpand();   // jump back to results page

public:
    QPushButton *m_loopBtn = nullptr;

private:
    QLabel      *m_titleLbl   = nullptr;
    QLabel      *m_channelLbl = nullptr;
    QLabel      *m_queueLbl   = nullptr;
    QPushButton *m_prevBtn    = nullptr;
    QPushButton *m_nextBtn    = nullptr;
    QPushButton *m_expandBtn  = nullptr;
};

// ── Main tab ──────────────────────────────────────────────────────────────────
class YoutubeTab : public QWidget {
    Q_OBJECT
public:
    explicit YoutubeTab(QWidget *parent = nullptr);

private slots:
    // navigation
    void onHomeSearch();
    void onTopSearch();
    void onBackHome();

    // playback
    void onCardActivated(int idx);
    void onCardAddToQueue(int idx);
    void onStream();
    void onDownload();
    void onNextInQueue();
    void onPrevInQueue();
    void onToggleLoop();
    void onCheckAutoNext();   // polls player for ended state

    // bridge
    void onSearchOutput();
    void onSearchFinished(int code, QProcess::ExitStatus);
    void onInfoOutput();
    void onInfoFinished(int code, QProcess::ExitStatus);
    void onQueueInfoOutput();
    void onQueueInfoFinished(int code, QProcess::ExitStatus);
    void onDownloadOutput();
    void onDownloadFinished(int code, QProcess::ExitStatus);

private:
    // ── Pages ─────────────────────────────────────────────────────────────────
    QStackedWidget *m_pages         = nullptr;  // 0=home, 1=results
    QWidget        *m_homePage      = nullptr;
    QWidget        *m_resultsPage   = nullptr;

    // ── Home ──────────────────────────────────────────────────────────────────
    QLineEdit      *m_homeSearch    = nullptr;
    QPushButton    *m_homeSearchBtn = nullptr;
    MiniPlayer     *m_miniPlayer    = nullptr;  // shown on home while playing

    // ── Results top bar ───────────────────────────────────────────────────────
    QLineEdit      *m_topSearch     = nullptr;
    QPushButton    *m_topSearchBtn  = nullptr;
    QPushButton    *m_backBtn       = nullptr;
    QLabel         *m_statusLabel   = nullptr;

    // ── Results grid ──────────────────────────────────────────────────────────
    QScrollArea    *m_gridScroll    = nullptr;
    QWidget        *m_gridContainer = nullptr;
    QGridLayout    *m_gridLayout    = nullptr;

    // ── Player panel ──────────────────────────────────────────────────────────
    QStackedWidget *m_playerStack   = nullptr;
    QWebEngineView *m_player        = nullptr;
    QLabel         *m_titleLabel    = nullptr;
    QLabel         *m_metaLabel     = nullptr;
    QTextEdit      *m_descView      = nullptr;
    QComboBox      *m_streamCombo   = nullptr;
    QPushButton    *m_streamBtn     = nullptr;
    QPushButton    *m_downloadBtn   = nullptr;
    QPushButton    *m_prevBtn       = nullptr;
    QPushButton    *m_nextBtn       = nullptr;
    QPushButton    *m_loopBtn       = nullptr;
    QProgressBar   *m_progress      = nullptr;
    QLabel         *m_progressLabel = nullptr;

    // ── Queue panel ───────────────────────────────────────────────────────────
    QListWidget    *m_queueList     = nullptr;
    QLabel         *m_queueLabel    = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    QNetworkAccessManager *m_nam    = nullptr;
    QList<YTResult> m_results;
    QList<YTStream> m_streams;
    YTResult        m_currentResult;

    // Queue
    QList<QueueEntry> m_queue;
    int               m_queueIndex  = -1;   // currently playing index
    bool              m_looping     = false;

    // Auto-next polling
    QTimer           *m_autoNextTimer = nullptr;
    bool              m_wasPlaying    = false;

    // Pending queue info fetch
    int               m_pendingQueueIdx = -1;
    QString           m_queueInfoBuf;

    // Processes
    QProcess *m_searchProc   = nullptr;
    QProcess *m_infoProc     = nullptr;
    QProcess *m_queueInfoProc = nullptr;
    QProcess *m_dlProc       = nullptr;
    QString   m_searchBuf, m_infoBuf, m_dlBuf;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildHomePage();
    void buildResultsPage();
    void doSearch(const QString &q);
    void populateResults(const QJsonArray &arr);
    void populateStreams(const QJsonObject &obj);
    void playQueueIndex(int idx);
    void fetchQueueItemInfo(int qIdx);
    void updateQueuePanel();
    void updateMiniPlayer();
    void updateNavButtons();
    void runBridge(QProcess *&proc, const QStringList &args,
                   const char *readySlot, const char *finishedSlot);
    void setStatus(const QString &msg, const QString &color = "#aaa");
    QString formatDuration(qint64 s) const;
    QString formatViews(qint64 v) const;
    QString jarPath() const;
    static QPushButton *makeBtn(const QString &label, const QString &fg,
                                const QString &bg, QWidget *parent);
};