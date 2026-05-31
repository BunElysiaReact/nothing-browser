#include "PiggyCookieInject.h"
#include "PiggyServer.h"
#include "Sessionmanager.h"
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDateTime>

// ─── Singleton ────────────────────────────────────────────────────────────────

static PiggyCookieInject *s_cookieInject = nullptr;

PiggyCookieInject *piggy_cookieInject() { return s_cookieInject; }

void piggy_cookieInjectInit(PiggyServer *srv, const QString &cookieFile) {
    if (!s_cookieInject)
        s_cookieInject = new PiggyCookieInject(srv, cookieFile, srv);
}

// ─── Constructor ──────────────────────────────────────────────────────────────

PiggyCookieInject::PiggyCookieInject(PiggyServer *srv,
                                      const QString &cookieFile,
                                      QObject *parent)
    : QObject(parent), m_srv(srv), m_cookieFile(cookieFile),
      m_lastCount(0) {}

// ─── watchTab ─────────────────────────────────────────────────────────────────

void PiggyCookieInject::watchTab(const QString &tabId,
                                  QWebEnginePage *page) {
    m_pages[tabId] = page;

    // loadStarted fires BEFORE page JS executes — this is the crucial
    // difference from SessionManager::load() which fires too late.
    // WAWeb's WebSocket auth check happens at loadFinished, so we must
    // have cookies in the store before that point.
    QObject::connect(page, &QWebEnginePage::loadStarted, this,
        [this, tabId]() {
            auto *page = m_pages.value(tabId, nullptr);
            if (page) injectCookies(tabId, page);
        });

    // Also inject immediately in case page is already loaded
    // (e.g. tab created after navigation already started)
    injectCookies(tabId, page);
}

// ─── unwatchTab ───────────────────────────────────────────────────────────────

void PiggyCookieInject::unwatchTab(const QString &tabId) {
    m_pages.remove(tabId);
}

// ─── reloadAndInject ──────────────────────────────────────────────────────────

void PiggyCookieInject::reloadAndInject(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) {
        qWarning() << "[PiggyCookieInject] reloadAndInject: "
                      "unknown tabId" << tabId;
        return;
    }
    injectCookies(tabId, page);
}

// ─── setFile ─────────────────────────────────────────────────────────────────

void PiggyCookieInject::setFile(const QString &file) {
    m_cookieFile = file;
    qDebug() << "[PiggyCookieInject] Cookie file changed to:"
             << m_cookieFile;
}

// ─── injectCookies ────────────────────────────────────────────────────────────

void PiggyCookieInject::injectCookies(const QString &tabId,
                                       QWebEnginePage *page) {
    QFile f(m_cookieFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[PiggyCookieInject] Cannot open:"
                   << m_cookieFile;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isArray()) {
        qWarning() << "[PiggyCookieInject] cookies.json is not "
                      "a JSON array:" << m_cookieFile;
        return;
    }

    QJsonArray arr = doc.array();
    auto *store    = page->profile()->cookieStore();
    int injected   = 0;
    int skipped    = 0;

    for (const QJsonValue &v : arr) {
        if (!v.isObject()) { skipped++; continue; }
        QJsonObject obj = v.toObject();

        QString name   = obj["name"].toString().trimmed();
        QString domain = obj["domain"].toString().trimmed();

        if (name.isEmpty() || domain.isEmpty()) {
            qWarning() << "[PiggyCookieInject] Skipping cookie "
                          "with missing name or domain";
            skipped++;
            continue;
        }

        QNetworkCookie cookie;
        cookie.setName(name.toUtf8());
        cookie.setValue(obj["value"].toString().toUtf8());

        // Domain MUST have leading dot for subdomain matching.
        // This is the #1 silent failure — .whatsapp.net vs whatsapp.net
        // WAWeb checks cookies on both web.whatsapp.com and
        // *.whatsapp.net so the dot is non-negotiable.
        QString normalizedDomain = domain;
        if (!normalizedDomain.startsWith('.'))
            normalizedDomain = "." + normalizedDomain;
        cookie.setDomain(normalizedDomain);

        cookie.setPath(obj["path"].toString("/"));
        cookie.setSecure(obj["secure"].toBool(false));
        cookie.setHttpOnly(obj["httpOnly"].toBool(false));

        // Handle expiry — accept both unix timestamp and ISO string
        QJsonValue expiryVal = obj["expires"];
        if (!expiryVal.isUndefined() && !expiryVal.isNull()) {
            if (expiryVal.isDouble()) {
                qint64 ts = (qint64)expiryVal.toDouble();
                if (ts > 0)
                    cookie.setExpirationDate(
                        QDateTime::fromSecsSinceEpoch(ts));
            } else if (expiryVal.isString()) {
                QDateTime dt = QDateTime::fromString(
                    expiryVal.toString(), Qt::ISODate);
                if (dt.isValid()) cookie.setExpirationDate(dt);
            }
        }

        // Build origin URL for the cookie store.
        // Strip the leading dot for the host part.
        QString host   = normalizedDomain.mid(1);
        QString scheme = cookie.isSecure() ? "https" : "http";
        QUrl origin(scheme + "://" + host);

        store->setCookie(cookie, origin);
        injected++;
    }

    m_lastCount = injected;

    // Broadcast event to Node
    QJsonObject ev;
    ev["type"]     = "event";
    ev["event"]    = "cookies:injected";
    ev["tabId"]    = tabId;
    ev["count"]    = injected;
    ev["skipped"]  = skipped;
    ev["file"]     = m_cookieFile;
    broadcastEvent(ev);

    qDebug() << "[PiggyCookieInject] Injected" << injected
             << "cookies (" << skipped << "skipped) from"
             << m_cookieFile << "into tab" << tabId;
}

// ─── broadcastEvent ───────────────────────────────────────────────────────────

void PiggyCookieInject::broadcastEvent(const QJsonObject &event) {
    QByteArray msg =
        QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_srv->clients()) {
        if (client &&
            client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleCookieInject(PiggyServer *srv, const QString &c,
                               const QJsonObject &payload,
                               QLocalSocket *client, const QString &id,
                               const QString &tabId)
{
    auto *ci = piggy_cookieInject();

    // ── cookieinject.reload ───────────────────────────────────────────────────
    if (c == "cookieinject.reload") {
        if (!ci) {
            srv->respond(client, id, false,
                "cookieinject not initialized — "
                "make sure cookies.json exists at startup");
            return true;
        }
        if (tabId.isEmpty() || !srv->tabs().contains(tabId)) {
            srv->respond(client, id, false,
                "cookieinject.reload requires valid tabId");
            return true;
        }
        ci->reloadAndInject(tabId);
        srv->respond(client, id, true,
            QString("re-injected %1 cookies")
                .arg(ci->lastInjectedCount()));
        return true;
    }

    // ── cookieinject.status ───────────────────────────────────────────────────
    if (c == "cookieinject.status") {
        if (!ci) {
            srv->respond(client, id, true,
                QJsonObject{
                    {"active",   false},
                    {"injected", 0},
                    {"file",     ""}
                });
            return true;
        }
        QJsonObject o;
        o["active"]   = true;
        o["file"]     = ci->cookieFile();
        o["injected"] = ci->lastInjectedCount();
        srv->respond(client, id, true, o);
        return true;
    }

    // ── cookieinject.setFile ──────────────────────────────────────────────────
    if (c == "cookieinject.setFile") {
        QString file = payload["file"].toString().trimmed();
        if (file.isEmpty()) {
            srv->respond(client, id, false, "file path required");
            return true;
        }
        if (!QFile::exists(file)) {
            srv->respond(client, id, false,
                "file not found: " + file);
            return true;
        }
        if (!ci) {
            // First time — init with this file
            piggy_cookieInjectInit(srv, file);
            srv->respond(client, id, true,
                "cookieinject initialized with: " + file);
        } else {
            ci->setFile(file);
            srv->respond(client, id, true,
                "cookie file updated: " + file);
        }
        return true;
    }

    // ── cookieinject.inject ───────────────────────────────────────────────────
    // Force inject into a specific tab right now, no reload needed
    if (c == "cookieinject.inject") {
        if (!ci) {
            srv->respond(client, id, false,
                "cookieinject not initialized");
            return true;
        }
        if (tabId.isEmpty() || !srv->tabs().contains(tabId)) {
            srv->respond(client, id, false,
                "cookieinject.inject requires valid tabId");
            return true;
        }
        ci->reloadAndInject(tabId);
        srv->respond(client, id, true,
            QString("injected %1 cookies into tab %2")
                .arg(ci->lastInjectedCount(), tabId));
        return true;
    }

    return false;
}