#include "PiggyServer.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>

struct HttpConn {
    QByteArray buf;
};

static QMap<QTcpSocket*, HttpConn> s_conns;

static bool parseHttp(const QByteArray &raw,
                       QString &outKey,
                       QByteArray &outBody) {
    int split = raw.indexOf("\r\n\r\n");
    if (split == -1) return false;

    QByteArray headerSection = raw.left(split);
    outBody = raw.mid(split + 4);

    for (const QByteArray &line : headerSection.split('\n')) {
        QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("x-piggy-key:")) {
            outKey = QString::fromUtf8(trimmed.mid(12).trimmed());
        }
    }
    return true;
}

static void rejectHttp(QTcpSocket *sock, int status, const QString &error) {
    QString statusText = status == 401 ? "Unauthorized" :
                         status == 400 ? "Bad Request"  : "Error";
    QJsonObject err;
    err["ok"]    = false;
    err["error"] = error;
    QByteArray body = QJsonDocument(err).toJson(QJsonDocument::Compact);
    QByteArray resp;
    resp += "HTTP/1.1 " + QByteArray::number(status) + " "
          + statusText.toUtf8() + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body;
    sock->write(resp);
    sock->flush();
    sock->disconnectFromHost();
}

void piggy_startHttp(PiggyServer *srv, const QString &apiKey) {
    auto *tcpServer = new QTcpServer(srv);

    QObject::connect(tcpServer, &QTcpServer::newConnection, srv,
        [srv, tcpServer, apiKey]() {
            while (tcpServer->hasPendingConnections()) {
                auto *sock = tcpServer->nextPendingConnection();
                s_conns[sock] = HttpConn{};

                QObject::connect(sock, &QTcpSocket::readyRead, srv,
                    [srv, sock, apiKey]() {
                        s_conns[sock].buf += sock->readAll();
                        QByteArray &buf = s_conns[sock].buf;

                        QString incomingKey;
                        QByteArray body;
                        if (!parseHttp(buf, incomingKey, body)) return;

                        // ── Key check ─────────────────────────────────────────
                        if (incomingKey.isEmpty() || incomingKey != apiKey) {
                            rejectHttp(sock, 401, "Unauthorized — invalid or missing X-Piggy-Key");
                            return;
                        }

                        // ── Health check ──────────────────────────────────────
                        if (body.trimmed() == "hello") {
                            QByteArray reply = "Hello! I am active. Start scraping.\n";
                            QByteArray resp;
                            resp += "HTTP/1.1 200 OK\r\n";
                            resp += "Content-Type: text/plain\r\n";
                            resp += "Content-Length: " + QByteArray::number(reply.size()) + "\r\n";
                            resp += "Connection: close\r\n\r\n";
                            resp += reply;
                            sock->write(resp);
                            sock->flush();
                            sock->disconnectFromHost();
                            return;
                        }

                        // ── JSON parse ────────────────────────────────────────
                        QJsonDocument doc = QJsonDocument::fromJson(body);
                        if (doc.isNull() || !doc.isObject()) {
                            rejectHttp(sock, 400, "Invalid JSON body");
                            return;
                        }

                        // ── Route to command handler ──────────────────────────
                        srv->handleHttpCommand(doc.object(), sock);
                    });

                QObject::connect(sock, &QTcpSocket::disconnected, srv, [sock]() {
                    s_conns.remove(sock);
                    sock->deleteLater();
                });
            }
        });

    if (!tcpServer->listen(QHostAddress::Any, 2005)) {
        qWarning() << "[PiggyHttp] Failed to bind port 2005:"
                   << tcpServer->errorString();
    }
}