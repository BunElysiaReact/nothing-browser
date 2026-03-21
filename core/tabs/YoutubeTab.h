#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QComboBox>
#include <QStackedWidget>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWebEngineView>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileDialog>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkReply>

struct YTResult {
    QString id, url, title, uploader, thumbnail;
    qint64  duration = 0, views = 0;
};

struct YTStream {
    QString type, quality, format, url;
    int     bitrate = 0, fps = 0;
};

class YoutubeTab : public QWidget {
    Q_OBJECT
public:
    explicit YoutubeTab(QWidget *parent = nullptr);

private slots:
    void onSearch();
    void onResultClicked(QListWidgetItem *item);
    void onResultDoubleClicked(QListWidgetItem *item);
    void onStream();
    void onDownload();

    void onSearchOutput();
    void onSearchFinished(int code, QProcess::ExitStatus);
    void onInfoOutput();
    void onInfoFinished(int code, QProcess::ExitStatus);
    void onDownloadOutput();
    void onDownloadFinished(int code, QProcess::ExitStatus);
    void onLoopToggled(bool checked);

private:
    // ── Top bar ───────────────────────────────────────────────────────────────
    QLineEdit   *m_searchInput  = nullptr;
    QPushButton *m_searchBtn    = nullptr;
    QLabel      *m_statusDot    = nullptr;

    // ── Results panel ─────────────────────────────────────────────────────────
    QLabel      *m_countBadge   = nullptr;
    QListWidget *m_resultsList  = nullptr;

    // ── Player ────────────────────────────────────────────────────────────────
    QWidget        *m_playerPlaceholder = nullptr;
    QWebEngineView *m_player            = nullptr;
    QStackedWidget *m_playerStack       = nullptr;
    QWidget        *m_scrubberBar       = nullptr;
    QWidget        *m_scrubberFill      = nullptr;

    // ── Info bar ──────────────────────────────────────────────────────────────
    QLabel *m_videoTitle    = nullptr;
    QLabel *m_metaUploader  = nullptr;
    QLabel *m_metaDuration  = nullptr;
    QLabel *m_metaViews     = nullptr;

    // ── Controls bar ──────────────────────────────────────────────────────────
    QComboBox   *m_qualitySelect    = nullptr;
    QPushButton *m_streamBtn        = nullptr;
    QPushButton *m_downloadBtn      = nullptr;
    QPushButton *m_loopBtn          = nullptr;
    QWidget     *m_dlProgressWrap   = nullptr;
    bool         m_looping          = false;
    QWidget     *m_progressFill     = nullptr;
    QLabel      *m_progressLabel    = nullptr;

    // ── Description ───────────────────────────────────────────────────────────
    QTextEdit *m_descText = nullptr;

    // ── Status bar ────────────────────────────────────────────────────────────
    QLabel *m_statusMsg = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    QList<YTResult> m_results;
    QList<YTStream> m_streams;
    YTResult        m_current;

    QProcess *m_searchProc  = nullptr;
    QProcess *m_infoProc    = nullptr;
    QProcess *m_dlProc      = nullptr;
    QString   m_searchBuf, m_infoBuf, m_dlBuf;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildUI();
    void runBridge(QProcess *&proc, const QStringList &args,
                   const char *readySlot, const char *finishedSlot);
    void populateResults(const QJsonArray &arr);
    void populateStreams(const QJsonObject &obj);
    void setStatus(const QString &msg, const QString &color = "#5a9e38");
    void setDotState(const QString &state);
    void setProgress(int pct);

    QString jarPath() const;
    QString formatDuration(qint64 s) const;
    QString formatViews(qint64 v) const;

    static QString globalStyle();
    static QPushButton *makeBtn(const QString &label, const QString &fg,
                                const QString &border, const QString &hoverBg,
                                QWidget *parent);
};