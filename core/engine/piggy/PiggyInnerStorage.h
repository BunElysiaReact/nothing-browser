#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMap>
#include <QFile>

class PiggyServer;
class QWebEnginePage;

// ─── localStorage / IndexedDB persistence ────────────────────────────────────

class PiggyInnerStorage : public QObject {
    Q_OBJECT
public:
    explicit PiggyInnerStorage(PiggyServer *srv,
                                const QString &path,
                                QObject *parent = nullptr);

    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);

    void injectIntoTab(const QString &tabId);

    // Public accessors for command handler
    QJsonObject dumpStorage() const  { return loadStorage(); }
    void        clearStorage()       { QFile(m_path).remove(); }

private slots:
    void onLoadFinished(bool ok, const QString &tabId);

private:
    void installStorageHook(const QString &tabId);
    void saveStorage(const QString &tabId, const QJsonObject &data);
    QJsonObject loadStorage() const;
    void broadcastEvent(const QJsonObject &event);

    PiggyServer                    *m_srv;
    QString                         m_path;
    QMap<QString, QWebEnginePage*>  m_pages;
};

bool piggy_handleStorage(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId);

PiggyInnerStorage *piggy_innerStorage();
void               piggy_innerStorageInit(PiggyServer *srv, const QString &path);