#include "PiggyServer.h"
#include "../NetworkCapture.h"
#include <QWebEnginePage>
#include <QJsonArray>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Capture control ──────────────────────────────────────────────────────────

void piggy_startCapture(PiggyServer *srv, const QString &tabId) {
    if (!srv->tabs().contains(tabId)) return;
    TabContext &ctx = srv->tabs()[tabId];
    if (!ctx.captureActive) {
        ctx.page->runJavaScript(NetworkCapture::captureScript());
        ctx.captureActive = true;
        qDebug() << "[PiggyServer] Capture started for tab" << tabId;
    }
}

void piggy_stopCapture(PiggyServer *srv, const QString &tabId) {
    if (!srv->tabs().contains(tabId)) return;
    srv->tabs()[tabId].captureActive = false;
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleCapture(PiggyServer *srv, const QString &c,
                          const QJsonObject & /*payload*/,
                          QLocalSocket *client, const QString &id,
                          const QString &tabId) {
    if (c == "capture.start") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        piggy_startCapture(srv, tabId);
        srv->respond(client, id, true, "capture started");
        return true;
    }

    if (c == "capture.stop") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        piggy_stopCapture(srv, tabId);
        srv->respond(client, id, true, "capture stopped");
        return true;
    }

    if (c == "capture.requests") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QJsonArray arr;
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
            arr.append(o);
        }
        srv->respond(client, id, true, arr);
        return true;
    }

    if (c == "capture.ws") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QJsonArray arr;
        for (const auto &f : srv->tabs()[tabId].capturedWsFrames) {
            QJsonObject o;
            o["connectionId"] = f.connectionId; o["url"]       = f.url;
            o["direction"]    = f.direction;    o["data"]      = f.data;
            o["binary"]       = f.isBinary;     o["timestamp"] = f.timestamp;
            arr.append(o);
        }
        srv->respond(client, id, true, arr);
        return true;
    }

    if (c == "capture.clear") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        srv->tabs()[tabId].capturedRequests.clear();
        srv->tabs()[tabId].capturedWsFrames.clear();
        srv->respond(client, id, true, "capture cleared");
        return true;
    }

    return false;
}