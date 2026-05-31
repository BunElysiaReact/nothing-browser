#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMap>

class PiggyServer;
class QWebEnginePage;

// ─── localStorage / IndexedDB persistence ────────────────────────────────────
// Snapshots localStorage to disk on every change via a MutationObserver-style
// JS hook injected at DocumentCreation. On tab load, re-injects saved state
// before WAWeb JS runs.
//
// Events:
//   { type:"event", event:"storage:saved", tabId, keys:int }
//   { type:"event", event:"storage:loaded", tabId, keys:int }
//
// Commands:
//   storage.get    { tabId, key }           → string|null
//   storage.set    { tabId, key, value }    → ok
//   storage.dump   { tabId }                → { key: value, ... }
//   storage.clear  { tabId }                → ok
// ─────────────────────────────────────────────────────────────────────────────

class PiggyInnerStorage : public QObject {
    Q_OBJECT
public:
    explicit PiggyInnerStorage(PiggyServer *srv,
                                const QString &path,
                                QObject *parent = nullptr);

    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);

    // Inject saved localStorage into page before WAWeb runs
    void injectIntoTab(const QString &tabId);

private slots:
    void onLoadFinished(bool ok, const QString &tabId);

private:
    void installStorageHook(const QString &tabId);
    void saveStorage(const QString &tabId, const QJsonObject &data);
    QJsonObject loadStorage() const;
    void broadcastEvent(const QJsonObject &event);

    PiggyServer                    *m_srv;
    QString                         m_path;   // path to storage .json file
    QMap<QString, QWebEnginePage*>  m_pages;
};

bool piggy_handleStorage(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId);

PiggyInnerStorage *piggy_innerStorage();
void               piggy_innerStorageInit(PiggyServer *srv, const QString &path);