#include "PiggyServer.h"
#include "PiggyProxy.h"
#include <QJsonObject>
#include <QLocalSocket>
#include <QJsonArray>
#include <QJsonDocument>

// ─── Forward declarations from split files ────────────────────────────────────
bool piggy_handleNavigation(PiggyServer *srv, const QString &c,
                             const QJsonObject &payload,
                             QLocalSocket *client, const QString &id,
                             const QString &tabId);

bool piggy_handleInteraction(PiggyServer *srv, const QString &c,
                              const QJsonObject &payload,
                              QLocalSocket *client, const QString &id,
                              const QString &tabId);

bool piggy_handleMedia(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id,
                        const QString &tabId);

bool piggy_handleCapture(PiggyServer *srv, const QString &c,
                          const QJsonObject &payload,
                          QLocalSocket *client, const QString &id,
                          const QString &tabId);

bool piggy_handleExport(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId);

// ─── Main command router ──────────────────────────────────────────────────────

void piggy_handleCommand(PiggyServer *srv, const QJsonObject &cmd, QLocalSocket *client) {
    const QString id      = cmd["id"].toString();
    const QString c       = cmd["cmd"].toString();
    const QJsonObject payload = cmd["payload"].toObject();
    const QString tabId   = payload["tabId"].toString();

    // ── Tab management ────────────────────────────────────────────────────────
    if (c == "tab.new") {
        srv->respond(client, id, true, srv->createTab());
        return;
    }
    if (c == "tab.close") {
        if (tabId.isEmpty()) { srv->respond(client, id, false, "tab.close requires tabId"); return; }
        srv->closeTab(tabId);
        srv->respond(client, id, true, "closed");
        return;
    }
    if (c == "tab.list") {
        QJsonArray arr;
        arr.append("default");
        for (const QString &k : srv->tabs().keys()) arr.append(k);
        srv->respond(client, id, true, arr.toVariantList());
        return;
    }

    // ── Proxy commands (proxy.*) ──────────────────────────────────────────────
    if (c.startsWith("proxy.")) {
        piggy_handleProxy(srv, c, payload, client, id);
        return;
    }

    // ── Navigation commands ───────────────────────────────────────────────────
    if (piggy_handleNavigation(srv, c, payload, client, id, tabId)) return;

    // ── Media commands ────────────────────────────────────────────────────────
    if (piggy_handleMedia(srv, c, payload, client, id, tabId)) return;

    // ── Capture commands ──────────────────────────────────────────────────────
    if (piggy_handleCapture(srv, c, payload, client, id, tabId)) return;

    // ── Interaction commands ──────────────────────────────────────────────────
    if (piggy_handleInteraction(srv, c, payload, client, id, tabId)) return;

    // ── Export / session / cookie / init script / expose commands ────────────
    if (piggy_handleExport(srv, c, payload, client, id, tabId)) return;

    // ── Unknown command ───────────────────────────────────────────────────────
    srv->respond(client, id, false, "unknown command: " + c);
}