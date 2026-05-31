#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMap>

class PiggyServer;
class QWebEnginePage;

// ─── Cookie injection BEFORE page loads ──────────────────────────────────────
// Hooks loadStarted (not constructor like SessionManager) so cookies are
// in the store before WAWeb's WebSocket auth check fires at loadFinished.
//
// Why SessionManager alone isn't enough:
//   SessionManager::load() → called in PiggyServer constructor
//   → sets cookies on profile store
//   → but Qt needs a domain context to accept cookies
//   → domain context only exists after first navigation starts
//   → by the time loadFinished fires, WAWeb auth already checked cookies
//   → result: session not restored, QR shown again
//
// This plugin fixes that by injecting on every loadStarted, ensuring
// cookies are always present with correct domain context.
//
// Events emitted to Node:
//   { type:"event", event:"cookies:injected", tabId,
//     count:int, skipped:int, file:string }
//
// Commands:
//   cookieinject.reload    { tabId }        re-read file + inject now
//   cookieinject.inject    { tabId }        alias for reload
//   cookieinject.status    {}               { active, file, injected }
//   cookieinject.setFile   { file }         change cookie file path
// ─────────────────────────────────────────────────────────────────────────────

class PiggyCookieInject : public QObject {
    Q_OBJECT
public:
    explicit PiggyCookieInject(PiggyServer *srv,
                                const QString &cookieFile,
                                QObject *parent = nullptr);

    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);

    void reloadAndInject(const QString &tabId);
    void setFile(const QString &file);

    QString cookieFile()       const { return m_cookieFile; }
    int     lastInjectedCount() const { return m_lastCount;  }

private:
    void injectCookies(const QString &tabId, QWebEnginePage *page);
    void broadcastEvent(const QJsonObject &event);

    PiggyServer                    *m_srv;
    QString                         m_cookieFile;
    int                             m_lastCount = 0;
    QMap<QString, QWebEnginePage*>  m_pages;
};

// ─── Singleton ────────────────────────────────────────────────────────────────
PiggyCookieInject *piggy_cookieInject();
void               piggy_cookieInjectInit(PiggyServer *srv,
                                          const QString &cookieFile);

// ─── Command handler ──────────────────────────────────────────────────────────
bool piggy_handleCookieInject(PiggyServer *srv, const QString &c,
                               const QJsonObject &payload,
                               QLocalSocket *client, const QString &id,
                               const QString &tabId);