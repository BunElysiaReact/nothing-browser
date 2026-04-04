#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>

// ── Changelog entry ───────────────────────────────────────────────────────────
struct ChangeEntry {
    QString type; // "added" | "fix" | "coming"
    QString text;
};

// ── Version info from GitHub Releases API ────────────────────────────────────
struct VersionInfo {
    QString            version;
    QString            downloadUrl;   // direct .tar.gz asset URL
    QString            releaseName;
    QString            releaseNotes;  // raw markdown body
    QList<ChangeEntry> changelog;     // parsed from body
    bool               isNewer = false;
};

// ═════════════════════════════════════════════════════════════════════════════
//  UpdateChecker
//  - Polls GitHub Releases API every 6 hours
//  - Downloads binary asset with progress reporting
//  - Writes updater.sh and executes it to swap binary + restart
// ═════════════════════════════════════════════════════════════════════════════
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    static constexpr const char *CURRENT_VERSION = "0.1.3";
    static constexpr const char *GITHUB_API_URL  =
        "https://api.github.com/repos/BunElysiaReact/nothing-browser/releases/latest";

    explicit UpdateChecker(QObject *parent = nullptr);

    void checkNow();
    void startDownload(const VersionInfo &info);
    void cancelDownload();

signals:
    void updateAvailable(const VersionInfo &info);
    void noUpdate(const VersionInfo &info);
    void checkFailed(const QString &error);

    // Download progress 0–100
    void downloadProgress(int percent);
    // Download finished — ready to install
    void downloadReady(const QString &archivePath, const VersionInfo &info);
    // Download failed
    void downloadFailed(const QString &error);
    // Install triggered (app about to exit)
    void installStarted();

public slots:
    void installUpdate(const QString &archivePath);

private slots:
    void onCheckReply(QNetworkReply *reply);
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    QNetworkAccessManager *m_nam        = nullptr;
    QTimer                *m_timer      = nullptr;
    QNetworkReply         *m_dlReply    = nullptr;
    QFile                 *m_dlFile     = nullptr;
    VersionInfo            m_pending;

    static int        versionToInt(const QString &v);
    static VersionInfo parseGithubRelease(const QByteArray &data);
    static QList<ChangeEntry> parseChangelog(const QString &body);
    QString           downloadDir() const;
};