#include "CdpProbe.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

CdpProbe::CdpProbe(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

void CdpProbe::start(const QString &debugHost, int debugPort) {
    // Initial target fetch
    refreshTargets();

    // Poll for new targets every 3 seconds (iframe/navigation discovery)
    auto *pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &CdpProbe::refreshTargets);
    pollTimer->start(3000);
}

void CdpProbe::refreshTargets() {
    QUrl url(QString("http://127.0.0.1:9222/json"));
    auto *reply = m_nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "[CDP] Failed to fetch targets:" << reply->errorString();
            reply->deleteLater();
            return;
        }
        pickBestTarget(reply->readAll(), "127.0.0.1", 9222);
        reply->deleteLater();
    });
}

void CdpProbe::pickBestTarget(const QByteArray &json, const QString &host, int port) {
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) return;

    // Prefer the most recently created page target
    QString bestWsUrl;
    int latestId = -1;
    for (const auto &v : doc.array()) {
        QJsonObject o = v.toObject();
        if (o["type"].toString() != "page") continue;
        int id = o["id"].toString().toInt();
        if (id > latestId) {
            latestId = id;
            bestWsUrl = o["webSocketDebuggerUrl"].toString();
        }
    }

    if (bestWsUrl.isEmpty()) return;

    // If already connected to this WS URL, skip
    if (m_ws && m_ws->requestUrl().toString() == bestWsUrl) return;

    // Disconnect old WS if any
    if (m_ws) {
        m_ws->deleteLater();
        m_ws = nullptr;
    }

    qDebug() << "[CDP] Connecting to" << bestWsUrl;
    m_ws = new QWebSocket();
    connect(m_ws, &QWebSocket::connected, this, &CdpProbe::onCdpConnected);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &CdpProbe::onCdpMessage);
    connect(m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, [](QAbstractSocket::SocketError) {
        qDebug() << "[CDP] WebSocket error";
    });
    m_ws->open(QUrl(bestWsUrl));
}

void CdpProbe::onCdpConnected() {
    qDebug() << "[CDP] Connected, enabling Network domain";
    QJsonObject cmd;
    cmd["id"] = m_nextId++;
    cmd["method"] = "Network.enable";
    m_ws->sendTextMessage(QJsonDocument(cmd).toJson(QJsonDocument::Compact));
}

void CdpProbe::onCdpMessage(const QString &message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    // Handle method events
    QString method = obj["method"].toString();
    QJsonObject params = obj["params"].toObject();

    if (method == "Network.webSocketCreated") {
        WebSocketFrame f;
        f.connectionId = params["requestId"].toString();
        f.url          = params["url"].toString();
        f.direction    = "OPEN";
        f.data         = "[WebSocket opened]";
        f.timestamp    = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        emit wsFrameCaptured(f);
        m_currentRequestId = f.connectionId; // store for later frames
    }
    else if (method == "Network.webSocketFrameSent" || method == "Network.webSocketFrameReceived") {
        QJsonObject resp = params["response"].toObject();
        WebSocketFrame f;
        f.connectionId = params["requestId"].toString();
        f.url          = "";  // CDP doesn't include URL here, but we can derive from stored requestId if needed
        f.direction    = (method == "Network.webSocketFrameSent") ? "UP SENT" : "DN RECV";
        f.data         = resp["payloadData"].toString();
        f.isBinary     = (resp["opcode"].toInt() == 2); // opcode 2 = binary
        f.timestamp    = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        emit wsFrameCaptured(f);
    }
    else if (method == "Network.webSocketClosed") {
        WebSocketFrame f;
        f.connectionId = params["requestId"].toString();
        f.direction    = "CLOSED";
        f.data         = "[Closed]";
        f.timestamp    = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        emit wsFrameCaptured(f);
    }
}

void CdpProbe::stop() {
    if (m_ws) {
        m_ws->close();
        m_ws->deleteLater();
        m_ws = nullptr;
    }
}