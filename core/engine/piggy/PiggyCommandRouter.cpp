#include "PiggyServer.h"
#include "PiggyProxy.h"
#include "PiggyFind.h"
#include "PiggyProvide.h"
#include "PiggyCaptcha.h"
#include "PiggyWait.h"
#include "PiggyDialog.h"
#include "PiggyIframe.h"
#include "PiggyHuman.h"
#include "PiggyQR.h"
#include "PiggyInnerStorage.h"
#include "PiggyCookieInject.h"
#include "PiggyMediaCapture.h"
#include <QJsonObject>
#include <QLocalSocket>
#include <QJsonArray>
#include <QJsonDocument>

// ─── Forward declarations ─────────────────────────────────────────────────────
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

void piggy_handleCommand(PiggyServer *srv, const QJsonObject &cmd,
                          QLocalSocket *client) {
    const QString id          = cmd["id"].toString();
    const QString c           = cmd["cmd"].toString();
    const QJsonObject payload = cmd["payload"].toObject();
    const QString tabId       = payload["tabId"].toString();

    // ── Tab management ────────────────────────────────────────────────────────
    if (c == "tab.new") {
        srv->respond(client, id, true, srv->createTab());
        return;
    }
    if (c == "tab.close") {
        if (tabId.isEmpty()) {
            srv->respond(client, id, false, "tab.close requires tabId");
            return;
        }
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

    // ── Proxy (proxy.*) ───────────────────────────────────────────────────────
    if (c.startsWith("proxy.")) {
        piggy_handleProxy(srv, c, payload, client, id);
        return;
    }

    // ── Find (find.*) ─────────────────────────────────────────────────────────
    if (c.startsWith("find.")) {
        piggy_handleFind(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Provide (provide.*) ───────────────────────────────────────────────────
    if (c.startsWith("provide.")) {
        piggy_handleProvide(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Captcha + Block (captcha.* / block.*) ─────────────────────────────────
    if (c.startsWith("captcha.") || c.startsWith("block.")) {
        piggy_handleCaptcha(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Dialog + Upload (dialog.* / upload) ───────────────────────────────────
    if (c.startsWith("dialog.") || c == "upload") {
        piggy_handleDialog(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Iframe (iframe.*) ─────────────────────────────────────────────────────
    if (c.startsWith("iframe.")) {
        piggy_handleIframe(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Human (human.*) ───────────────────────────────────────────────────────
    if (c.startsWith("human.")) {
        piggy_handleHuman(srv, c, payload, client, id, tabId);
        return;
    }

    // ── QR (qr.*) ─────────────────────────────────────────────────────────────
    if (c.startsWith("qr.")) {
        piggy_handleQR(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Storage (storage.*) ───────────────────────────────────────────────────
    if (c.startsWith("storage.")) {
        piggy_handleStorage(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Media capture (media.*) ───────────────────────────────────────────────
    if (c.startsWith("media.")) {
        piggy_handleMediaCapture(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Cookie inject commands (cookieinject.*) ───────────────────────────────
    if (c.startsWith("cookieinject.")) {
        piggy_handleCookieInject(srv, c, payload, client, id, tabId);
        return;
    }

    // ── Extended wait + fetch ─────────────────────────────────────────────────
    if (c == "wait.function"   ||
        c == "wait.selector"   ||
        c == "fetch.textAll"   ||
        c == "fetch.attr"      ||
        c == "fetch.attrAll"   ||
        (c == "evaluate" && payload.contains("timeout")))
    {
        if (piggy_handleWait(srv, c, payload, client, id, tabId)) return;
    }

    // ── Navigation ────────────────────────────────────────────────────────────
    if (piggy_handleNavigation(srv, c, payload, client, id, tabId)) return;

    // ── Media (screenshot / pdf / image blocking) ─────────────────────────────
    if (piggy_handleMedia(srv, c, payload, client, id, tabId)) return;

    // ── Capture ───────────────────────────────────────────────────────────────
    if (piggy_handleCapture(srv, c, payload, client, id, tabId)) return;

    // ── Human type override ───────────────────────────────────────────────────
    if (c == "type" && payload["clear"].toBool(false)) {
        if (piggy_handleHuman(srv, c, payload, client, id, tabId)) return;
    }

    // ── Interaction ───────────────────────────────────────────────────────────
    if (piggy_handleInteraction(srv, c, payload, client, id, tabId)) return;

    // ── Export / session / cookie / init script / expose ─────────────────────
    if (piggy_handleExport(srv, c, payload, client, id, tabId)) return;

    // ── Wait (remaining) ──────────────────────────────────────────────────────
    if (piggy_handleWait(srv, c, payload, client, id, tabId)) return;

    // ── Unknown ───────────────────────────────────────────────────────────────
    srv->respond(client, id, false, "unknown command: " + c);
}