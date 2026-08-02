#include "PiggyServer.h"
#include "Sessionmanager.h"
#include "../../tabs/PiggyTab.h"
#include "../NetworkCapture.h"
#include "../Interceptor.h"
#include "../FingerprintSpoofer.h"
#include "../ProxyManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QNetworkRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QTextStream>

void piggy_handleCommand(PiggyServer *srv, const QJsonObject &cmd, QWebSocket *client);
QString piggy_createTab(PiggyServer *srv, QWebSocket *owner);
void    piggy_closeTab(PiggyServer *srv, const QString &tabId);
QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);
void    piggy_configureProfile(QWebEngineProfile *profile);
void    piggy_wireProxyEvents(PiggyServer *srv);

// ─── Constructors ─────────────────────────────────────────────────────────────

PiggyServer::PiggyServer(PiggyTab *piggy, QObject *parent)
    : QObject(parent), m_piggy(piggy)
{
    if (!m_piggy) {
        m_ownProfile = new QWebEngineProfile("piggy-persistent", this);
        piggy_configureProfile(m_ownProfile);
        m_ownPage = new QWebEnginePage(m_ownProfile, this);
    }
    QWebEngineProfile *profile = m_piggy ? m_piggy->getPage()->profile() : m_ownProfile;
    m_session = new SessionManager(profile, this);
    m_session->load();
    piggy_wireProxyEvents(this);
}

PiggyServer::PiggyServer(QWebEnginePage *page, QObject *parent)
    : QObject(parent), m_piggy(nullptr), m_headfulPage(page)
{
    m_session = new SessionManager(page->profile(), this);
    m_session->load();
    piggy_wireProxyEvents(this);
}

PiggyServer::~PiggyServer() { stop(); }

// ─── Public wrappers ──────────────────────────────────────────────────────────

QString PiggyServer::createTab(QWebSocket *owner)      { return piggy_createTab(this, owner); }
void    PiggyServer::closeTab(const QString &id)        { piggy_closeTab(this, id); }
QWebEnginePage* PiggyServer::page(const QString &tabId) { return piggy_page(this, tabId); }

// ─── Server lifecycle ──────────────────────────────────────────────────────────
//
// One WebSocket server, one fixed port (2005), forever. Any script that
// wants in either finds this already listening and joins it, or is the one
// that launched the binary in the first place — either way it ends up here.

void PiggyServer::start(const QString &apiKey) {
    m_apiKey = apiKey;

    m_server = new QWebSocketServer(
        QStringLiteral("piggy"), QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(QHostAddress::Any, PORT)) {
        qWarning() << "[PiggyServer] Failed to bind port" << PORT << "—"
                   << m_server->errorString()
                   << "(another Piggy instance is probably already running "
                      "on this machine — scripts should just connect to it "
                      "instead of spawning a new one)";
        return;
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &PiggyServer::onNewConnection);

    QTextStream out(stdout);
    out << "\n";
    out << "  ┌─────────────────────────────────────────┐\n";
    out << "  │  [Piggy] WebSocket daemon config          │\n";
    out << "  ├─────────────────────────────────────────┤\n";
    out << "  │  port : " << PORT << "  (fixed — never changes)\n";
    out << "  │  auth : " << (apiKey.isEmpty() ? QStringLiteral("none (local/open)")
                                                 : QStringLiteral("X-Piggy-Key required")) << "\n";
    out << "  │  url  : ws://127.0.0.1:" << PORT << "\n";
    out << "  └─────────────────────────────────────────┘\n\n";
    out.flush();

    qInfo() << "[Piggy] Listening on ws://0.0.0.0:" << PORT
            << "(shared — multiple scripts can connect at once)";
}

void PiggyServer::stop() {
    if (m_server) { m_server->close(); m_server = nullptr; }
    for (auto &ctx : m_tabs) {
        ctx.page->deleteLater();
        ctx.interceptor->deleteLater();
        ctx.capture->deleteLater();
    }
    m_tabs.clear();
    m_tabOwner.clear();
    m_clientTabs.clear();
}

// ─── Connection handling ───────────────────────────────────────────────────────

void PiggyServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QWebSocket *client = m_server->nextPendingConnection();

        // ── Key check (only enforced if a key was configured) ──────────────
        if (!m_apiKey.isEmpty()) {
            const QString incomingKey =
                QString::fromUtf8(client->request().rawHeader("X-Piggy-Key"));
            if (incomingKey != m_apiKey) {
                qWarning() << "[PiggyServer] Rejected connection — bad/missing X-Piggy-Key";
                client->close(QWebSocketProtocol::CloseCodePolicyViolated,
                              "Unauthorized — invalid or missing X-Piggy-Key");
                client->deleteLater();
                continue;
            }
        }

        m_clients.append(client);
        m_clientTabs[client] = {};
        connect(client, &QWebSocket::textMessageReceived,
                this, &PiggyServer::onClientTextMessage);
        connect(client, &QWebSocket::disconnected,
                this, &PiggyServer::onClientDisconnected);

        // Explicit ready ack, sent as a normal text frame — NOT relying on
        // the WS "open" event alone, since a bad-key rejection also opens
        // the socket for a moment before we close it. Clients should wait
        // for this message (or a close) before treating the connection as
        // usable.
        QJsonObject ready;
        ready["type"] = "ready";
        client->sendTextMessage(QJsonDocument(ready).toJson(QJsonDocument::Compact));

        qDebug() << "[PiggyServer] Client connected — total clients:" << m_clients.size();
    }
}

void PiggyServer::onClientDisconnected() {
    auto *client = qobject_cast<QWebSocket*>(sender());
    if (!client) return;

    // Close out (but don't auto-close if flagged "noclose") every tab this
    // client owned, so a script disconnecting doesn't leak browser tabs
    // forever, but also doesn't blow away work another script still cares
    // about, since ownership is per-client.
    const QSet<QString> owned = m_clientTabs.value(client);
    for (const QString &tabId : owned) {
        if (m_tabs.contains(tabId) && !m_tabs[tabId].noClose) {
            closeTab(tabId);
        }
        m_tabOwner.remove(tabId);
    }
    m_clientTabs.remove(client);

    m_clients.removeAll(client);
    client->deleteLater();
    qDebug() << "[PiggyServer] Client disconnected — total clients:" << m_clients.size();
}

void PiggyServer::onClientTextMessage(const QString &message) {
    auto *client = qobject_cast<QWebSocket*>(sender());
    if (!client) return;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        respond(client, "", false, "Invalid JSON");
        return;
    }
    handleCommand(doc.object(), client);
}

void PiggyServer::handleCommand(const QJsonObject &cmd, QWebSocket *client) {
    piggy_handleCommand(this, cmd, client);
}

// ─── respond() / broadcast() / sendToOwner() ──────────────────────────────────

static QJsonValue variantToJson(const QVariant &data) {
    if (data.typeId() == QMetaType::QString) return data.toString();
    if (data.canConvert<QJsonArray>())  return data.value<QJsonArray>();
    if (data.canConvert<QJsonObject>()) return data.value<QJsonObject>();
    QJsonDocument d = QJsonDocument::fromVariant(data);
    if (d.isNull())       return data.toString();
    if (d.isArray())      return d.array();
    return d.object();
}

void PiggyServer::respond(QWebSocket *client, const QString &id,
                           bool ok, const QVariant &data) {
    if (!client || client->state() != QAbstractSocket::ConnectedState) return;

    QJsonObject res;
    res["id"]   = id;
    res["ok"]   = ok;
    res["data"] = variantToJson(data);
    client->sendTextMessage(QJsonDocument(res).toJson(QJsonDocument::Compact));
}

void PiggyServer::broadcast(const QJsonObject &event) {
    const QString msg = QJsonDocument(event).toJson(QJsonDocument::Compact);
    for (auto *client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState)
            client->sendTextMessage(msg);
    }
}

void PiggyServer::setTabOwner(const QString &tabId, QWebSocket *owner) {
    if (!owner) return;
    m_tabOwner[tabId] = owner;
    m_clientTabs[owner].insert(tabId);
}

void PiggyServer::sendToOwner(const QString &tabId, const QJsonObject &event) {
    QWebSocket *owner = m_tabOwner.value(tabId, nullptr);
    if (!owner) { broadcast(event); return; } // no known owner — fall back
    if (owner->state() != QAbstractSocket::ConnectedState) return;
    owner->sendTextMessage(QJsonDocument(event).toJson(QJsonDocument::Compact));
}

// ─── Capture slots ────────────────────────────────────────────────────────────

void PiggyServer::onRequestCaptured(const CapturedRequest &req, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive) m_tabs[tabId].capturedRequests.append(req);
}

void PiggyServer::onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive) m_tabs[tabId].capturedWsFrames.append(frame);
}

void PiggyServer::onCookieCaptured(const CapturedCookie &cookie, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    m_tabs[tabId].capturedCookies.append(cookie);
}

void PiggyServer::onCookieRemoved(const QString &name, const QString &domain,
                                   const QString &tabId) {
    Q_UNUSED(name); Q_UNUSED(domain); Q_UNUSED(tabId);
}

void PiggyServer::onStorageCaptured(const QString &origin, const QString &key,
                                     const QString &value, const QString &storageType,
                                     const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive)
        m_tabs[tabId].storageEntries.append({storageType+":"+origin+":"+key, value});
}

void PiggyServer::onExposedFunctionCalled(const QString &name, const QString &callId,
                                           const QString &data, const QString &tabId) {
    QJsonObject event;
    event["type"]   = "event";
    event["event"]  = "exposed_call";
    event["tabId"]  = tabId;
    event["name"]   = name;
    event["callId"] = callId;
    event["data"]   = data;
    // Tab-scoped: only the script that owns this tab should get the call.
    sendToOwner(tabId, event);
}
