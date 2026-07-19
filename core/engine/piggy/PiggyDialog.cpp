#include "PiggyServer.h"
#include "PiggyPage.h"
#include <QWebEnginePage>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QEventLoop>
#include <QTimer>

// Declared in PiggyTabManager.cpp
QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

static PiggyPage* piggy_dialogPage(PiggyServer *srv, const QString &tabId) {
    return qobject_cast<PiggyPage*>(piggy_page(srv, tabId));
}

// Safely quotes a string for embedding as a single-quoted JS string literal.
static QString piggy_jsQuote(const QString &s) {
    QString escaped = s;
    escaped.replace('\\', "\\\\").replace('\'', "\\'").replace('\n', "\\n");
    return "'" + escaped + "'";
}

// Blocks up to timeoutMs waiting for a dialog to become pending.
// Returns true if a dialog is pending by the time it returns (either
// already was, or arrived within the timeout).
static bool piggy_waitForDialogPending(PiggyPage *page, int timeoutMs) {
    if (page->hasPending()) return true;

    QEventLoop loop;
    bool arrived = false;

    QMetaObject::Connection conn = QObject::connect(
        page, &PiggyPage::dialogRequested,
        &loop, [&]() { arrived = true; loop.quit(); });

    QTimer::singleShot(timeoutMs, &loop, [&]() { loop.quit(); });
    loop.exec();

    QObject::disconnect(conn);
    return arrived || page->hasPending();
}

bool piggy_handleDialog(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId) {

    // ── dialog.accept / dialog.dismiss ────────────────────────────────────────
    if (c == "dialog.accept" || c == "dialog.dismiss") {
        PiggyPage *page = piggy_dialogPage(srv, tabId);
        if (!page) { srv->respond(client, id, false, "invalid tabId"); return true; }
        if (!page->hasPending()) { srv->respond(client, id, false, "no pending dialog"); return true; }

        bool accept = (c == "dialog.accept");
        QString text = payload["text"].toString();
        page->resolvePending(accept, text);
        srv->respond(client, id, true, accept ? "accepted" : "dismissed");
        return true;
    }

    // ── dialog.status ──────────────────────────────────────────────────────────
    if (c == "dialog.status") {
        PiggyPage *page = piggy_dialogPage(srv, tabId);
        if (!page) { srv->respond(client, id, false, "invalid tabId"); return true; }

        QJsonObject status;
        status["pending"] = page->hasPending();
        status["type"]    = page->pendingType();
        status["message"] = page->pendingMessage();
        status["autoAction"] = page->autoAction();
        srv->respond(client, id, true, status.toVariantMap());
        return true;
    }

    // ── dialog.setAutoAction ────────────────────────────────────────────────────
    if (c == "dialog.setAutoAction") {
        PiggyPage *page = piggy_dialogPage(srv, tabId);
        if (!page) { srv->respond(client, id, false, "invalid tabId"); return true; }

        QString action = payload["action"].toString();
        if (action != "accept" && action != "dismiss" && action != "manual") {
            srv->respond(client, id, false, "action must be accept|dismiss|manual");
            return true;
        }
        page->setAutoAction(action);
        srv->respond(client, id, true, "autoAction set to " + action);
        return true;
    }

    // ── dialog.waitAndAccept / dialog.waitAndDismiss ────────────────────────────
    if (c == "dialog.waitAndAccept" || c == "dialog.waitAndDismiss") {
        PiggyPage *page = piggy_dialogPage(srv, tabId);
        if (!page) { srv->respond(client, id, false, "invalid tabId"); return true; }

        int timeout = payload["timeout"].toInt(30000);
        if (!piggy_waitForDialogPending(page, timeout)) {
            srv->respond(client, id, false, "timed out waiting for dialog");
            return true;
        }

        // Between the poll returning and here, hasPending() is still true —
        // we're on the same thread, nothing else can resolve it in between.
        bool accept = (c == "dialog.waitAndAccept");
        QString text = payload["text"].toString();
        QString message = page->pendingMessage();
        page->resolvePending(accept, text);

        QJsonObject result;
        result["message"] = message;
        result["action"]  = accept ? "accepted" : "dismissed";
        srv->respond(client, id, true, result.toVariantMap());
        return true;
    }

    // ── upload ───────────────────────────────────────────────────────────────
    if (c == "upload") {
        PiggyPage *page = piggy_dialogPage(srv, tabId);
        if (!page) { srv->respond(client, id, false, "invalid tabId"); return true; }

        QString selector = payload["selector"].toString();
        QString path      = payload["path"].toString();
        if (selector.isEmpty() || path.isEmpty()) {
            srv->respond(client, id, false, "upload requires selector and path");
            return true;
        }

        // chooseFiles() is synchronous — stage the path, then click the
        // input so Qt triggers the native picker (which we intercept).
        page->setPendingUploadPath(path);

        QString js = QString(
            "(function(){"
            "  var el = document.querySelector(%1);"
            "  if (!el) return 'not_found';"
            "  el.click();"
            "  return 'clicked';"
            "})()"
        ).arg(piggy_jsQuote(selector));

        page->runJavaScript(js, [srv, client, id](const QVariant &v) {
            if (v.toString() == "not_found") {
                srv->respond(client, id, false, "selector not found");
            } else {
                srv->respond(client, id, true, "uploaded");
            }
        });
        return true;
    }

    return false;
}