#include "PiggyServer.h"
#include "../NetworkCapture.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

// ─── Export handler ───────────────────────────────────────────────────────────

bool piggy_handleExport(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId) {
    if (c == "export.json") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }

        QJsonArray reqArr;
        for (const auto &req : srv->tabs()[tabId].capturedRequests) {
            QJsonObject o;
            o["method"] = req.method; o["url"]  = req.url;
            o["status"] = req.status; o["type"] = req.type;
            o["mime"]   = req.mimeType;
            o["reqHeaders"] = req.requestHeaders;
            o["reqBody"]    = req.requestBody;
            o["resHeaders"] = req.responseHeaders;
            o["resBody"]    = req.responseBody;
            o["size"]       = req.size;
            o["timestamp"]  = req.timestamp;
            reqArr.append(o);
        }

        QJsonArray wsArr;
        for (const auto &f : srv->tabs()[tabId].capturedWsFrames) {
            QJsonObject o;
            o["connectionId"] = f.connectionId; o["url"]       = f.url;
            o["direction"]    = f.direction;    o["data"]      = f.data;
            o["binary"]       = f.isBinary;     o["timestamp"] = f.timestamp;
            wsArr.append(o);
        }

        QJsonObject out;
        out["requests"] = reqArr;
        out["websocket"] = wsArr;

        if (payload.contains("path")) {
            QFile file(payload["path"].toString());
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(out).toJson());
                file.close();
                srv->respond(client, id, true, "export written to file");
                return true;
            }
            srv->respond(client, id, false, "failed to open file for writing");
            return true;
        }

        srv->respond(client, id, true, out);
        return true;
    }

    return false;
}