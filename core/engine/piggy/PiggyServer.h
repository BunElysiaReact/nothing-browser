#pragma once
#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineUrlRequestInterceptor>
#include <QNetworkCookie>
#include <QMap>
#include <QVector>
#include <QList>
#include <QSet>
#include <QUuid>
#include <QDir>
#include "../NetworkCapture.h"

class PiggyTab;
class Interceptor;
class SessionManager;

struct InterceptRule {
    QString urlPattern;
    bool block = false;
    QString redirectUrl;
    QMap<QString, QString> setHeaders;
    QMap<QString, QString> removeHeaders;
};

struct TabContext {
    QWebEnginePage   *page             = nullptr;
    Interceptor      *interceptor      = nullptr;
    NetworkCapture   *capture          = nullptr;
    QStringList       initScripts;
    bool              imageBlocked     = false;
    bool              captureActive    = false;
    bool              exposedConnected = false;
    bool              noClose          = false; // set via "noclose" command — tab must not be auto-closed
    QStringList       exposedFunctions;
    QVector<InterceptRule>           rules;
    QList<CapturedRequest>           capturedRequests;
    QList<WebSocketFrame>            capturedWsFrames;
    QList<CapturedCookie>            capturedCookies;
    QList<QPair<QString, QString>>   storageEntries;
};

// ─── PiggyServer ────────────────────────────────────────────────────────────
//
// Single WebSocket server, fixed forever on port 2005. Every JS-side script
// — whether it spawned the binary itself or just found one already running —
// talks to the exact same endpoint: ws://127.0.0.1:2005 (or a remote host,
// same port, with an X-Piggy-Key header for auth). Any number of scripts can
// be connected at once; the binary is a shared daemon, not a 1:1 pipe.
//
// Each connected QWebSocket is a "client". Tabs are owned by whichever
// client created them (see m_tabOwner) so that tab-scoped events (navigate,
// dialog, exposed_call, …) are routed only to the script that owns that tab,
// not blasted to every connected script. Global events (proxy.*, etc.) are
// broadcast to everyone, since proxy state is process-wide.
//
class PiggyServer : public QObject {
    Q_OBJECT
public:
    explicit PiggyServer(PiggyTab *piggy, QObject *parent = nullptr);
    explicit PiggyServer(QWebEnginePage *page, QObject *parent = nullptr);
    ~PiggyServer();

    // Starts the one and only WebSocket server. Port is NEVER configurable —
    // 2005, always. apiKey is optional: if non-empty, every incoming
    // connection must present a matching "X-Piggy-Key" header or it is
    // rejected during the handshake. Prints a config block to the terminal.
    void start(const QString &apiKey = QString());
    void stop();

    QMap<QString, TabContext>  &tabs()        { return m_tabs; }
    QList<QWebSocket*>         &clients()     { return m_clients; }
    PiggyTab                   *piggy()       { return m_piggy; }
    QWebEnginePage              *headfulPage() { return m_headfulPage; }
    QWebEngineProfile          *ownProfile()  { return m_ownProfile; }
    QWebEnginePage              *ownPage()     { return m_ownPage; }
    SessionManager              *session()     { return m_session; }

    // ── Responses & events ──────────────────────────────────────────────
    void respond(QWebSocket *client, const QString &id,
        bool ok, const QVariant &data = QVariant());

    // Send a raw event object to every connected client.
    void broadcast(const QJsonObject &event);
    // Send a raw event object only to the client that owns this tab.
    // Falls back to broadcast() if the tab has no known owner (e.g. the
    // implicit "default" tab, which predates any client connecting).
    void sendToOwner(const QString &tabId, const QJsonObject &event);

    QWebEnginePage* page(const QString &tabId = QString());
    QString         createTab(QWebSocket *owner = nullptr);
    void            closeTab(const QString &tabId);

    // Records that `owner` created `tabId`, so future tab-scoped events
    // route only to that client and get cleaned up if it disconnects.
    // No-op if owner is nullptr (e.g. the implicit "default" tab).
    void setTabOwner(const QString &tabId, QWebSocket *owner);

    // Port is a compile-time constant on purpose: every script and every
    // binary instance agrees on it without any configuration step.
    static constexpr quint16 PORT = 2005;

signals:
    void tabCreated(const QString &tabId, QWebEnginePage *page);
    void tabClosed(const QString &tabId);

public slots:
    void onRequestCaptured(const CapturedRequest &req, const QString &tabId);
    void onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId);
    void onCookieCaptured(const CapturedCookie &cookie, const QString &tabId);
    void onCookieRemoved(const QString &name, const QString &domain, const QString &tabId);
    void onStorageCaptured(const QString &origin, const QString &key,
        const QString &value, const QString &storageType,
        const QString &tabId);
    void onExposedFunctionCalled(const QString &name, const QString &callId,
        const QString &data, const QString &tabId);

private slots:
    void onNewConnection();
    void onClientTextMessage(const QString &message);
    void onClientDisconnected();

private:
    void handleCommand(const QJsonObject &cmd, QWebSocket *client);

    PiggyTab          *m_piggy           = nullptr;
    QWebEnginePage    *m_headfulPage     = nullptr;
    QWebSocketServer  *m_server          = nullptr;
    QWebEngineProfile *m_ownProfile      = nullptr;
    QWebEnginePage    *m_ownPage         = nullptr;
    SessionManager    *m_session         = nullptr;
    QString            m_apiKey;

    QMap<QString, TabContext> m_tabs;
    QList<QWebSocket*>        m_clients;

    // Ownership bookkeeping — who gets tab-scoped events, and whose tabs
    // get cleaned up when they disconnect.
    QMap<QString, QWebSocket*> m_tabOwner;   // tabId   -> owning client
    QMap<QWebSocket*, QSet<QString>> m_clientTabs; // client -> owned tabIds
};
