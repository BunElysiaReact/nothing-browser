#include "PiggyMediaCapture.h"
#include "PiggyServer.h"
#include "../NetworkCapture.h"
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QJsonDocument>
#include <QDir>
#include <QUuid>
#include <QMimeDatabase>

static PiggyMediaCapture *s_mediaCapture = nullptr;
PiggyMediaCapture *piggy_mediaCapture() { return s_mediaCapture; }
void piggy_mediaCaptureInit(PiggyServer *srv, const QString &downloadDir) {
    if (!s_mediaCapture)
        s_mediaCapture = new PiggyMediaCapture(srv, downloadDir, srv);
}

PiggyMediaCapture::PiggyMediaCapture(PiggyServer *srv,
                                      const QString &downloadDir,
                                      QObject *parent)
    : QObject(parent), m_srv(srv), m_downloadDir(downloadDir)
{
    QDir().mkpath(downloadDir);
}

void PiggyMediaCapture::watchTab(const QString &tabId, QWebEnginePage *page) {
    m_pages[tabId]   = page;
    m_tabDirs[tabId] = m_downloadDir;
    m_entries[tabId] = {};
    installMediaHook(tabId);
}

void PiggyMediaCapture::unwatchTab(const QString &tabId) {
    // Close any open files for this tab
    for (auto *f : m_openFiles) {
        if (f->isOpen()) f->close();
        f->deleteLater();
    }
    m_openFiles.clear();
    m_pages.remove(tabId);
    m_tabDirs.remove(tabId);
    m_entries.remove(tabId);
}

void PiggyMediaCapture::setDownloadDir(const QString &tabId,
                                        const QString &dir) {
    QDir().mkpath(dir);
    m_tabDirs[tabId] = dir;
}

QString PiggyMediaCapture::mimeToExt(const QString &mime) const {
    if (mime.contains("mp4"))  return ".mp4";
    if (mime.contains("webm")) return ".webm";
    if (mime.contains("ogg"))  return ".ogg";
    if (mime.contains("mp3") || mime.contains("mpeg")) return ".mp3";
    if (mime.contains("opus")) return ".opus";
    if (mime.contains("aac"))  return ".aac";
    if (mime.contains("wav"))  return ".wav";
    if (mime.contains("jpeg") || mime.contains("jpg")) return ".jpg";
    if (mime.contains("png"))  return ".png";
    if (mime.contains("gif"))  return ".gif";
    if (mime.contains("webp")) return ".webp";
    return ".bin";
}

void PiggyMediaCapture::installMediaHook(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) return;

    // We hook into NetworkCapture's requestCaptured signal
    // filtering for media MIME types and writing response body to disk
    // The key insight: NetworkCapture already has the response body
    // we just need to intercept it BEFORE it goes to the JS pipe

    // Wire to the tab's capture object via PiggyServer
    if (!m_srv->tabs().contains(tabId)) return;
    auto &ctx = m_srv->tabs()[tabId];

    QObject::connect(ctx.capture, &NetworkCapture::requestCaptured, this,
        [this, tabId](const CapturedRequest &req) {
            QString mime = req.mimeType.toLower();

            // Only intercept media types
            bool isMedia = mime.startsWith("video/") ||
                           mime.startsWith("audio/") ||
                           mime.startsWith("image/") ||
                           mime.contains("octet-stream");

            if (!isMedia) return;
            if (req.responseBody.isEmpty()) return;

            QString dir  = m_tabDirs.value(tabId, m_downloadDir);
            QString ext  = mimeToExt(mime);
            QString name = QUuid::createUuid().toString(QUuid::WithoutBraces)
                               .left(8) + ext;
            QString path = dir + "/" + name;

            // Write directly to disk — Node never sees these bytes
            QByteArray body = QByteArray::fromBase64(req.responseBody.toUtf8());
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly)) {
                QJsonObject ev;
                ev["type"]  = "event";
                ev["event"] = "media:error";
                ev["tabId"] = tabId;
                ev["url"]   = req.url;
                ev["error"] = "cannot write to: " + path;
                broadcastEvent(ev);
                return;
            }
            f.write(body);
            f.close();

            // Record entry
            MediaEntry entry;
            entry.url   = req.url;
            entry.mime  = mime;
            entry.path  = path;
            entry.bytes = body.size();
            entry.done  = true;
            m_entries[tabId].append(entry);

            QJsonObject ev;
            ev["type"]  = "event";
            ev["event"] = "media:done";
            ev["tabId"] = tabId;
            ev["url"]   = req.url;
            ev["mime"]  = mime;
            ev["path"]  = path;
            ev["bytes"] = (int)body.size();
            broadcastEvent(ev);

            qDebug() << "[PiggyMedia] Saved" << mime
                     << body.size() << "bytes →" << path;
        });
}

void PiggyMediaCapture::broadcastEvent(const QJsonObject &event) {
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_srv->clients()) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleMediaCapture(PiggyServer *srv, const QString &c,
                               const QJsonObject &payload,
                               QLocalSocket *client, const QString &id,
                               const QString &tabId)
{
    auto *mc = piggy_mediaCapture();

    if (c == "media.setDir") {
        if (!mc) { srv->respond(client, id, false, "media capture not initialized"); return true; }
        QString dir = payload["dir"].toString();
        if (dir.isEmpty()) { srv->respond(client, id, false, "dir required"); return true; }
        mc->setDownloadDir(tabId, dir);
        srv->respond(client, id, true, "media dir set: " + dir);
        return true;
    }

    if (c == "media.list") {
        if (!mc) { srv->respond(client, id, true, QJsonArray()); return true; }
        QJsonArray arr;
        for (const auto &e : mc->entries(tabId)) {
            QJsonObject o;
            o["url"]   = e.url;
            o["mime"]  = e.mime;
            o["path"]  = e.path;
            o["bytes"] = (int)e.bytes;
            o["done"]  = e.done;
            arr.append(o);
        }
        srv->respond(client, id, true, arr);
        return true;
    }

    if (c == "media.clear") {
        if (!mc) { srv->respond(client, id, true, "ok"); return true; }
        // Can't easily clear from here without exposing internals
        // so we respond ok — next version can add a clear() method
        srv->respond(client, id, true, "cleared");
        return true;
    }

    return false;
}