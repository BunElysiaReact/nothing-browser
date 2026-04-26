#include "PiggyServer.h"
#include "Sessionmanager.h"
#include "../../tabs/PiggyTab.h"
#include "../NetworkCapture.h"
#include "../Interceptor.h"
#include "../FingerprintSpoofer.h"
#include "../ProxyManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QTcpSocket>
#include <QWebEnginePage>
#include <QWebEngineProfile>

void piggy_handleCommand(PiggyServer *srv, const QJsonObject &cmd, QLocalSocket *client);
QString piggy_createTab(PiggyServer *srv);
void    piggy_closeTab(PiggyServer *srv, const QString &tabId);
QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);
void    piggy_configureProfile(QWebEngineProfile *profile);
void    piggy_wireProxyEvents(PiggyServer *srv);
void    piggy_startHttp(PiggyServer *srv, const QString &apiKey);

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

QString PiggyServer::createTab()                        { return piggy_createTab(this); }
void    PiggyServer::closeTab(const QString &id)        { piggy_closeTab(this, id); }
QWebEnginePage* PiggyServer::page(const QString &tabId) { return piggy_page(this, tabId); }

void PiggyServer::startHttp(const QString &apiKey) {
    m_apiKey = apiKey;
    piggy_startHttp(this, apiKey);
}

// ─── Socket server lifecycle ──────────────────────────────────────────────────

void PiggyServer::start() {
    QLocalServer::removeServer(SOCKET_NAME);
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection,
            this, &PiggyServer::onNewConnection);
    if (!m_server->listen(SOCKET_NAME)) {
        qWarning() << "[PiggyServer] Failed to start:" << m_server->errorString();
        return;
    }
    qDebug() << "[PiggyServer] Listening on socket:" << SOCKET_NAME;
    qDebug() << "[PiggyServer] Working directory:" << SessionManager::workDir();
    qDebug() << "[PiggyServer] Cookies:" << SessionManager::cookiesPath();
    qDebug() << "[PiggyServer] Profile:" << SessionManager::profilePath();
}

void PiggyServer::stop() {
    if (m_server) { m_server->close(); m_server = nullptr; }
    for (auto &ctx : m_tabs) {
        ctx.page->deleteLater();
        ctx.interceptor->deleteLater();
        ctx.capture->deleteLater();
    }
    m_tabs.clear();
}

// ─── Socket connection handling ───────────────────────────────────────────────

void PiggyServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        auto *client = m_server->nextPendingConnection();
        m_clients.append(client);
        connect(client, &QLocalSocket::readyRead,    this, &PiggyServer::onClientData);
        connect(client, &QLocalSocket::disconnected, this, &PiggyServer::onClientDisconnected);
        qDebug() << "[PiggyServer] Socket client connected";
    }
}

void PiggyServer::onClientDisconnected() {
    auto *client = qobject_cast<QLocalSocket*>(sender());
    if (client) { m_clients.removeAll(client); client->deleteLater(); }
}

void PiggyServer::onClientData() {
    auto *client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;
    QByteArray raw = client->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isNull() || !doc.isObject()) {
        respond(client, "", false, "Invalid JSON");
        return;
    }
    handleCommand(doc.object(), client);
}

void PiggyServer::handleCommand(const QJsonObject &cmd, QLocalSocket *client) {
    piggy_handleCommand(this, cmd, client);
}

// ─── HTTP command entry point ─────────────────────────────────────────────────

void PiggyServer::handleHttpCommand(const QJsonObject &cmd, QTcpSocket *sock) {
    m_pendingHttpSock = sock;
    piggy_handleCommand(this, cmd, nullptr);
    m_pendingHttpSock = nullptr;
}

// ─── respond() — routes to socket or HTTP ────────────────────────────────────

void PiggyServer::respond(QLocalSocket *client, const QString &id,
                           bool ok, const QVariant &data) {
    if (m_pendingHttpSock) {
        respondHttp(m_pendingHttpSock, id, ok, data);
        return;
    }
    if (!client || client->state() != QLocalSocket::ConnectedState) return;

    QJsonObject res;
    res["id"] = id;
    res["ok"] = ok;
    if (data.typeId() == QMetaType::QString) {
        res["data"] = data.toString();
    } else if (data.canConvert<QJsonArray>()) {
        res["data"] = data.value<QJsonArray>();
    } else if (data.canConvert<QJsonObject>()) {
        res["data"] = data.value<QJsonObject>();
    } else {
        QJsonDocument d = QJsonDocument::fromVariant(data);
        if (d.isNull())        res["data"] = data.toString();
        else if (d.isArray())  res["data"] = d.array();
        else                   res["data"] = d.object();
    }
    client->write(QJsonDocument(res).toJson(QJsonDocument::Compact) + "\n");
    client->flush();
}

void PiggyServer::respondHttp(QTcpSocket *sock, const QString &id,
                               bool ok, const QVariant &data) {
    QJsonObject res;
    res["id"] = id;
    res["ok"] = ok;
    if (data.typeId() == QMetaType::QString) {
        res["data"] = data.toString();
    } else if (data.canConvert<QJsonArray>()) {
        res["data"] = data.value<QJsonArray>();
    } else if (data.canConvert<QJsonObject>()) {
        res["data"] = data.value<QJsonObject>();
    } else {
        QJsonDocument d = QJsonDocument::fromVariant(data);
        if (d.isNull())        res["data"] = data.toString();
        else if (d.isArray())  res["data"] = d.array();
        else                   res["data"] = d.object();
    }

    QByteArray body = QJsonDocument(res).toJson(QJsonDocument::Compact);
    QByteArray resp;
    resp += "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    sock->write(resp);
    sock->flush();
    sock->disconnectFromHost();
}

// ─── Capture slots ────────────────────────────────────────────────────────────

void PiggyServer::onRequestCaptured(const CapturedRequest &req, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive) m_tabs[tabId].capturedRequests.append(req);
}

void PiggyServer::onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive) {
        m_tabs[tabId].capturedWsFrames.append(frame);

        // Persist to ws.json in cwd if opt-in is on
        if (m_session && m_session->saveWs()) {
            QJsonObject entry;
            entry["tabId"]        = tabId;
            entry["connectionId"] = frame.connectionId;
            entry["url"]          = frame.url;
            entry["direction"]    = frame.direction;
            entry["data"]         = frame.data;
            entry["binary"]       = frame.isBinary;
            entry["timestamp"]    = frame.timestamp;
            m_session->appendWsFrame(entry);
        }
    }
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
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_clients) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}