#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonArray>

// ── Represents one changelog entry ───────────────────────────────────────────
struct ChangeEntry {
    QString type; // "added" | "fix" | "coming"
    QString text;
};

struct VersionInfo {
    QString         version;
    QString         downloadUrl;
    QList<ChangeEntry> changelog;
    bool            isNewer = false;
};

// ── Polls your hosted version.json every 6 hours ─────────────────────────────
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    static constexpr const char *CURRENT_VERSION = "0.1.0";
    // ↓ replace with your real hosted URL when you have one
    static constexpr const char *VERSION_URL =
        "https://raw.githubusercontent.com/BunElysiaReact/nothing-browser/main/version.json";

    explicit UpdateChecker(QObject *parent = nullptr);
    void checkNow();

signals:
    void updateAvailable(const VersionInfo &info);
    void noUpdate(const VersionInfo &info);   // still fires with changelog
    void checkFailed(const QString &error);

private slots:
    void onReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam;
    QTimer                *m_timer;

    static int  versionToInt(const QString &v);   // "0.2.1" → 201
    static VersionInfo parseJson(const QByteArray &data);
};