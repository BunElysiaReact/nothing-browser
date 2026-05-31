#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMap>
#include <QFile>

class PiggyServer;
class QWebEnginePage;

// ─── Media stream interceptor ─────────────────────────────────────────────────
// Intercepts media downloads (video/audio/images) at the network level
// and writes them directly to disk via C++ without passing bytes through
// the JS pipe. Keeps Node RAM flat regardless of file size.
//
// Events:
//   { type:"event", event:"media:start",    tabId, url, mime, path }
//   { type:"event", event:"media:done",     tabId, url, path, bytes }
//   { type:"event", event:"media:error",    tabId, url, error }
//
// Commands:
//   media.setDir   { tabId, dir }     → set download directory
//   media.list     { tabId }          → list downloaded files
//   media.clear    { tabId }          → clear download list
// ─────────────────────────────────────────────────────────────────────────────

struct MediaEntry {
    QString url;
    QString mime;
    QString path;
    qint64  bytes = 0;
    bool    done  = false;
};

class PiggyMediaCapture : public QObject {
    Q_OBJECT
public:
    explicit PiggyMediaCapture(PiggyServer *srv,
                                const QString &downloadDir,
                                QObject *parent = nullptr);

    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);
    void setDownloadDir(const QString &tabId, const QString &dir);

    QList<MediaEntry> entries(const QString &tabId) const {
        return m_entries.value(tabId);
    }

private:
    void installMediaHook(const QString &tabId);
    void broadcastEvent(const QJsonObject &event);
    QString mimeToExt(const QString &mime) const;

    PiggyServer                         *m_srv;
    QString                              m_downloadDir;
    QMap<QString, QWebEnginePage*>       m_pages;
    QMap<QString, QList<MediaEntry>>     m_entries;
    QMap<QString, QString>               m_tabDirs;
    QMap<QString, QFile*>                m_openFiles;
};

bool piggy_handleMediaCapture(PiggyServer *srv, const QString &c,
                               const QJsonObject &payload,
                               QLocalSocket *client, const QString &id,
                               const QString &tabId);

PiggyMediaCapture *piggy_mediaCapture();
void               piggy_mediaCaptureInit(PiggyServer *srv,
                                          const QString &downloadDir);