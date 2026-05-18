#include "PiggyServer.h"
#include "../NetworkCapture.h"
#include <QWebEnginePage>
#include <QJsonArray>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Capture control ──────────────────────────────────────────────────────────

void piggy_startCapture(PiggyServer *srv, const QString &tabId) {
    if (!srv->tabs().contains(tabId)) return;
    TabContext &ctx = srv->tabs()[tabId];
    if (ctx.captureActive) return;
    ctx.captureActive = true;

    // Inject into current page
    ctx.page->runJavaScript(NetworkCapture::captureScript());

    // Re-inject on every navigation via init script
    QWebEngineScript script;
    script.setName("nothing_capture_" + tabId);
    script.setSourceCode(NetworkCapture::captureScript());
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    ctx.page->profile()->scripts()->insert(script);

    // Connect signals to populate capturedRequests
    if (!ctx.captureConnected) {
        QObject::connect(ctx.capture, &NetworkCapture::requestCaptured, srv,
            [srv, tabId](const CapturedRequest &req) {
                if (!srv->tabs().contains(tabId)) return;
                if (!srv->tabs()[tabId].captureActive) return;
                srv->tabs()[tabId].capturedRequests.append(req);
            });
        QObject::connect(ctx.capture, &NetworkCapture::wsFrameCaptured, srv,
            [srv, tabId](const WebSocketFrame &f) {
                if (!srv->tabs().contains(tabId)) return;
                if (!srv->tabs()[tabId].captureActive) return;
                srv->tabs()[tabId].capturedWsFrames.append(f);
            });
        QObject::connect(ctx.capture, &NetworkCapture::cookieCaptured, srv,
            [srv, tabId](const CapturedCookie &ck) {
                if (!srv->tabs().contains(tabId)) return;
                srv->tabs()[tabId].capturedCookies.append(ck);
            });
        QObject::connect(ctx.capture, &NetworkCapture::storageCaptured, srv,
            [srv, tabId](const QString &, const QString &key, const QString &value, const QString &) {
                if (!srv->tabs().contains(tabId)) return;
                srv->tabs()[tabId].storageEntries.append({key, value});
            });
        ctx.captureConnected = true;
    }

    qDebug() << "[PiggyServer] Capture started for tab" << tabId;
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

    if (c == "capture.cookies") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QJsonArray arr;
        for (const auto &ck : srv->tabs()[tabId].capturedCookies) {
            QJsonObject o;
            o["name"]     = ck.name;   o["value"]    = ck.value;
            o["domain"]   = ck.domain; o["path"]     = ck.path;
            o["httpOnly"] = ck.httpOnly; o["secure"]  = ck.secure;
            o["expires"]  = ck.expires;
            arr.append(o);
        }
        srv->respond(client, id, true, arr);
        return true;
    }

    if (c == "capture.storage") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QJsonArray arr;
        for (const auto &pair : srv->tabs()[tabId].storageEntries) {
            QJsonObject o;
            o["key"] = pair.first; o["value"] = pair.second;
            arr.append(o);
        }
        srv->respond(client, id, true, arr);
        return true;
    }

    if (c == "capture.clear") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        srv->tabs()[tabId].capturedRequests.clear();
        srv->tabs()[tabId].capturedWsFrames.clear();
        srv->tabs()[tabId].capturedCookies.clear();
        srv->tabs()[tabId].storageEntries.clear();
        srv->respond(client, id, true, "capture cleared");
        return true;
    }

    return false;
}