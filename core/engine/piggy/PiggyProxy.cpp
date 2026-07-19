#include "PiggyProxy.h"
#include "PiggyServer.h"
#include "../ProxyManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QFile>
#include <QTextStream>

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Broadcast an event JSON to all connected clients
static void broadcastEvent(PiggyServer *srv, const QJsonObject &event) {
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : srv->clients()) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// Convert a ProxyEntry to a JSON summary object
static QJsonObject entryToJson(const ProxyEntry &e) {
    QJsonObject o;
    o["host"]    = e.host;
    o["port"]    = e.port;
    o["type"]    = (e.type == ProxyEntry::HTTP)  ? "http"  :
                   (e.type == ProxyEntry::HTTPS)  ? "https" : "socks5";
    o["user"]    = e.user;
    o["proxy"]   = e.toString();
    o["latency"] = e.latency;
    o["health"]  = (e.health == ProxyEntry::Alive)    ? "alive"    :
                   (e.health == ProxyEntry::Dead)     ? "dead"     :
                   (e.health == ProxyEntry::Checking) ? "checking" : "unchecked";
    return o;
}

// ─── Wire ProxyManager signals → client broadcast events ─────────────────────

void piggy_wireProxyEvents(PiggyServer *srv) {
    auto &pm = ProxyManager::instance();

    // proxy changed (rotation / manual switch)
    QObject::connect(&pm, &ProxyManager::proxyChanged, srv,
        [srv](const ProxyEntry &e) {
            QJsonObject ev;
            ev["type"]    = "event";
            ev["event"]   = "proxy:changed";
            ev["proxy"]   = e.toString();
            ev["host"]    = e.host;
            ev["port"]    = e.port;
            ev["latency"] = e.latency;
            broadcastEvent(srv, ev);
        });

    // proxy list loaded (after load/fetch)
    QObject::connect(&pm, &ProxyManager::proxyListLoaded, srv,
        [srv](int count) {
            QJsonObject ev;
            ev["type"]  = "event";
            ev["event"] = "proxy:loaded";
            ev["count"] = count;
            broadcastEvent(srv, ev);
        });

    // fetch failed
    QObject::connect(&pm, &ProxyManager::fetchFailed, srv,
        [srv](const QString &error) {
            QJsonObject ev;
            ev["type"]  = "event";
            ev["event"] = "proxy:fetch:failed";
            ev["error"] = error;
            broadcastEvent(srv, ev);
        });

    // health check started
    QObject::connect(&pm, &ProxyManager::checkStarted, srv,
        [srv](int total) {
            QJsonObject ev;
            ev["type"]  = "event";
            ev["event"] = "proxy:check:started";
            ev["total"] = total;
            broadcastEvent(srv, ev);
        });

    // per-proxy health result (fires for every proxy tested)
    QObject::connect(&pm, &ProxyManager::checkProgress, srv,
        [srv](int index, ProxyEntry::Health result, int latencyMs) {
            QJsonObject ev;
            ev["type"]    = "event";
            ev["event"]   = result == ProxyEntry::Alive ? "proxy:alive" : "proxy:dead";
            ev["index"]   = index;
            ev["latency"] = latencyMs;
            broadcastEvent(srv, ev);
        });

    // all checks done → also auto-switch to first alive proxy
    QObject::connect(&pm, &ProxyManager::checkFinished, srv,
        [srv](int alive, int dead) {
            QJsonObject ev;
            ev["type"]  = "event";
            ev["event"] = "proxy:check:done";
            ev["alive"] = alive;
            ev["dead"]  = dead;
            // If no alive proxies remain, emit exhausted event
            if (alive == 0) {
                QJsonObject ex;
                ex["type"]  = "event";
                ex["event"] = "proxy:exhausted";
                broadcastEvent(srv, ex);
            }
            broadcastEvent(srv, ev);
        });

    // OVPN loaded
    QObject::connect(&pm, &ProxyManager::ovpnLoaded, srv,
        [srv](const QString &remote, int port) {
            QJsonObject ev;
            ev["type"]   = "event";
            ev["event"]  = "proxy:ovpn:loaded";
            ev["remote"] = remote;
            ev["port"]   = port;
            broadcastEvent(srv, ev);
        });
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleProxy(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id) {
    auto &pm = ProxyManager::instance();

    if (!c.startsWith("proxy.")) return false;

    // ── proxy.load — load from file path ──────────────────────────────────────
    if (c == "proxy.load") {
        QString path = payload["path"].toString();
        if (path.isEmpty()) { srv->respond(client, id, false, "path required"); return true; }
        bool ok = pm.loadFromFile(path);
        srv->respond(client, id, ok,
                     ok ? QString("loaded %1 proxies").arg(pm.count())
                        : "failed to open file: " + path);
        return true;
    }

    // ── proxy.fetch — fetch from URL ──────────────────────────────────────────
    if (c == "proxy.fetch") {
        QString url = payload["url"].toString();
        if (url.isEmpty()) { srv->respond(client, id, false, "url required"); return true; }
        pm.fetchFromUrl(url);
        // Response comes async via proxy:loaded / proxy:fetch:failed events
        srv->respond(client, id, true, "fetch started");
        return true;
    }

    // ── proxy.ovpn — load .ovpn config file ───────────────────────────────────
    if (c == "proxy.ovpn") {
        QString path = payload["path"].toString();
        if (path.isEmpty()) { srv->respond(client, id, false, "path required"); return true; }
        bool ok = pm.loadOvpnFile(path);
        srv->respond(client, id, ok, ok ? "ovpn loaded" : "failed to parse ovpn file");
        return true;
    }

    // ── proxy.set — set a single proxy inline ─────────────────────────────────
    if (c == "proxy.set") {
        // Accepts: { host, port, type?, user?, pass? }
        // OR:      { proxy: "socks5://user:pass@host:port" }
        ProxyEntry e;
        if (payload.contains("proxy")) {
            e = ProxyEntry::fromString(payload["proxy"].toString());
        } else {
            e.host = payload["host"].toString();
            e.port = (quint16)payload["port"].toInt(1080);
            e.user = payload["user"].toString();
            e.pass = payload["pass"].toString();
            QString type = payload["type"].toString("socks5").toLower();
            e.type = (type == "http")  ? ProxyEntry::HTTP  :
                     (type == "https") ? ProxyEntry::HTTPS : ProxyEntry::SOCKS5;
        }
        if (!e.isValid()) { srv->respond(client, id, false, "invalid proxy"); return true; }
        // Load as a single-entry pool
        QVector<ProxyEntry> list;
        list.append(e);
        // ProxyManager doesn't have setList() — so write to a temp string and reload
        // We apply directly instead:
        QNetworkProxy::setApplicationProxy(e.toQProxy());
        srv->respond(client, id, true, "proxy set: " + e.toString());
        return true;
    }

    // ── proxy.test — health-check all proxies ─────────────────────────────────
    if (c == "proxy.test") {
        if (pm.count() == 0) { srv->respond(client, id, false, "no proxies loaded"); return true; }
        pm.checkAll();
        // Results come via proxy:alive / proxy:dead / proxy:check:done events
        srv->respond(client, id, true, QString("testing %1 proxies").arg(pm.count()));
        return true;
    }

    // ── proxy.test.stop ───────────────────────────────────────────────────────
    if (c == "proxy.test.stop") {
        pm.stopChecking();
        srv->respond(client, id, true, "check aborted");
        return true;
    }

    // ── proxy.next / proxy.rotate ─────────────────────────────────────────────
    if (c == "proxy.next" || c == "proxy.rotate") {
        if (pm.count() == 0) { srv->respond(client, id, false, "no proxies loaded"); return true; }
        pm.next();
        srv->respond(client, id, true, pm.current().toString());
        return true;
    }

    // ── proxy.disable — use real IP ───────────────────────────────────────────
    if (c == "proxy.disable") {
        pm.disable();
        srv->respond(client, id, true, "proxy disabled — using real IP");
        return true;
    }

    // ── proxy.enable — re-activate current proxy ──────────────────────────────
    if (c == "proxy.enable") {
        pm.enableCurrent();
        srv->respond(client, id, true, "proxy enabled: " + pm.current().toString());
        return true;
    }

    // ── proxy.current — return current proxy details ──────────────────────────
    if (c == "proxy.current") {
        ProxyEntry e = pm.current();
        if (!e.isValid()) {
            srv->respond(client, id, true, QJsonObject{{"active", false}});
        } else {
            QJsonObject o = entryToJson(e);
            o["active"] = pm.isActive();
            srv->respond(client, id, true, o);
        }
        return true;
    }

    // ── proxy.stats — alive/dead/total/index ──────────────────────────────────
    if (c == "proxy.stats") {
        QJsonObject o;
        o["total"]   = pm.count();
        o["alive"]   = pm.aliveCount();
        o["dead"]    = pm.deadCount();
        o["index"]   = pm.currentIndex();
        o["active"]  = pm.isActive();
        o["checking"]= pm.isChecking();
        o["skipDead"]= pm.skipDead();
        o["autoCheck"]= pm.autoCheck();
        srv->respond(client, id, true, o);
        return true;
    }

    // ── proxy.list — full list with health info ───────────────────────────────
    if (c == "proxy.list") {
        QVector<ProxyEntry> proxies = pm.proxies();
        int limit = payload["limit"].toInt(500); // cap at 500 by default
        QJsonArray arr;
        int current = pm.currentIndex();
        for (int i = 0; i < qMin(proxies.size(), limit); i++) {
            QJsonObject o = entryToJson(proxies[i]);
            o["index"]   = i;
            o["current"] = (i == current);
            arr.append(o);
        }
        QJsonObject res;
        res["proxies"] = arr;
        res["total"]   = proxies.size();
        res["shown"]   = arr.size();
        srv->respond(client, id, true, res);
        return true;
    }

    // ── proxy.rotation — set rotation mode ───────────────────────────────────
    if (c == "proxy.rotation") {
        QString mode    = payload["mode"].toString("none").toLower();
        int     interval= payload["interval"].toInt(60); // seconds
        ProxyRotation r = (mode == "timed")      ? ProxyRotation::Timed      :
                          (mode == "perrequest")  ? ProxyRotation::PerRequest :
                                                    ProxyRotation::None;
        pm.setRotation(r, interval);
        srv->respond(client, id, true,
                     QString("rotation set to %1 (interval: %2s)").arg(mode).arg(interval));
        return true;
    }

    // ── proxy.config — set flags ──────────────────────────────────────────────
    if (c == "proxy.config") {
        if (payload.contains("skipDead"))  pm.setSkipDead(payload["skipDead"].toBool());
        if (payload.contains("autoCheck")) pm.setAutoCheck(payload["autoCheck"].toBool());
        QJsonObject o;
        o["skipDead"]  = pm.skipDead();
        o["autoCheck"] = pm.autoCheck();
        srv->respond(client, id, true, o);
        return true;
    }

    // ── proxy.save — save current proxy list to file ──────────────────────────
    if (c == "proxy.save") {
        QString path = payload["path"].toString();
        if (path.isEmpty()) { srv->respond(client, id, false, "path required"); return true; }
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            srv->respond(client, id, false, "cannot write to: " + path);
            return true;
        }
        QTextStream out(&f);
        QString filter = payload["filter"].toString("all").toLower(); // all | alive | dead
        for (const auto &e : pm.proxies()) {
            if (filter == "alive" && e.health != ProxyEntry::Alive) continue;
            if (filter == "dead"  && e.health != ProxyEntry::Dead)  continue;
            out << e.toString() << "\n";
        }
        srv->respond(client, id, true, QString("saved to %1").arg(path));
        return true;
    }

    return false; // unrecognised proxy.* command
}