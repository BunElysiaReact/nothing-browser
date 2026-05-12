#include "PiggyWait.h"
#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QTimer>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── helper: poll until JS expr is truthy ─────────────────────────────────────
struct PollCtx {
    int elapsed   = 0;
    int timeout   = 10000;
    int interval  = 100;
    bool done     = false;
};

bool piggy_handleWait(PiggyServer *srv, const QString &c,
                      const QJsonObject &payload,
                      QLocalSocket *client, const QString &id,
                      const QString &tabId)
{
    auto *p = piggy_page(srv, tabId);

    // ── wait.function ─────────────────────────────────────────────────────────
    if (c == "wait.function") {
        QString js  = payload["js"].toString();
        int timeout = payload["timeout"].toInt(10000);

        auto *ctx   = new PollCtx();
        ctx->timeout = timeout;
        auto *timer = new QTimer(srv);
        timer->setInterval(100);

        QObject::connect(timer, &QTimer::timeout, srv,
            [srv, client, id, p, js, timer, ctx]() mutable {
                if (ctx->done) return;
                ctx->elapsed += 100;

                p->runJavaScript(js,
                    [srv, client, id, timer, ctx](const QVariant &r) {
                        if (ctx->done) return;
                        if (r.toBool()) {
                            ctx->done = true;
                            timer->stop(); timer->deleteLater(); delete ctx;
                            srv->respond(client, id, true, "condition met");
                        } else if (ctx->elapsed >= ctx->timeout) {
                            ctx->done = true;
                            timer->stop(); timer->deleteLater(); delete ctx;
                            srv->respond(client, id, false, "wait.function timeout");
                        }
                    });
            });
        timer->start();
        return true;
    }

    // ── wait.selector (extended — supports state param) ───────────────────────
    if (c == "wait.selector") {
        QString selector = payload["selector"].toString();
        QString state    = payload["state"].toString("attached"); // attached|detached|visible|hidden
        int timeout      = payload["timeout"].toInt(10000);

        selector.replace("'", "\\'");

        // Build the condition JS based on state
        QString condJs;
        if (state == "attached" || state == "present") {
            condJs = QString("!!document.querySelector('%1')").arg(selector);
        } else if (state == "detached") {
            condJs = QString("!document.querySelector('%1')").arg(selector);
        } else if (state == "visible") {
            condJs = QString(
                "(function(){"
                "var el=document.querySelector('%1');"
                "if(!el) return false;"
                "var s=window.getComputedStyle(el);"
                "return s.display!=='none' && s.visibility!=='hidden' && s.opacity!=='0';"
                "})()"
            ).arg(selector);
        } else if (state == "hidden") {
            condJs = QString(
                "(function(){"
                "var el=document.querySelector('%1');"
                "if(!el) return true;" // detached = also hidden
                "var s=window.getComputedStyle(el);"
                "return s.display==='none' || s.visibility==='hidden' || s.opacity==='0';"
                "})()"
            ).arg(selector);
        } else {
            // fallback
            condJs = QString("!!document.querySelector('%1')").arg(selector);
        }

        auto *ctx   = new PollCtx();
        ctx->timeout = timeout;
        auto *timer = new QTimer(srv);
        timer->setInterval(100);

        QObject::connect(timer, &QTimer::timeout, srv,
            [srv, client, id, p, condJs, selector, state, timer, ctx]() mutable {
                if (ctx->done) return;
                ctx->elapsed += 100;

                p->runJavaScript(condJs,
                    [srv, client, id, selector, state, timer, ctx](const QVariant &r) {
                        if (ctx->done) return;
                        if (r.toBool()) {
                            ctx->done = true;
                            timer->stop(); timer->deleteLater(); delete ctx;
                            srv->respond(client, id, true, "selector " + state);
                        } else if (ctx->elapsed >= ctx->timeout) {
                            ctx->done = true;
                            timer->stop(); timer->deleteLater(); delete ctx;
                            srv->respond(client, id, false,
                                "timeout: selector '" + selector + "' never reached state '" + state + "'");
                        }
                    });
            });
        timer->start();
        return true;
    }

    // ── evaluate (with timeout) ───────────────────────────────────────────────
    if (c == "evaluate") {
        QString js  = payload["js"].toString();
        int timeout = payload["timeout"].toInt(0); // 0 = no timeout

        if (timeout <= 0) {
            // No timeout, run directly
            p->runJavaScript(js,
                [srv, client, id](const QVariant &r) {
                    srv->respond(client, id, true, r);
                });
            return true;
        }

        // With timeout: race between JS completion and QTimer
        auto *done  = new bool(false);
        auto *timer = new QTimer(srv);
        timer->setSingleShot(true);
        timer->setInterval(timeout);

        QObject::connect(timer, &QTimer::timeout, srv,
            [srv, client, id, done, timer]() {
                if (*done) return;
                *done = true;
                timer->deleteLater(); delete done;
                srv->respond(client, id, false, "evaluate timeout");
            });
        timer->start();

        p->runJavaScript(js,
            [srv, client, id, done, timer](const QVariant &r) {
                if (*done) return;
                *done = true;
                timer->stop(); timer->deleteLater(); delete done;
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── fetch.textAll ─────────────────────────────────────────────────────────
    if (c == "fetch.textAll") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        p->runJavaScript(
            QString(
                "Array.from(document.querySelectorAll('%1'))"
                ".map(function(el){ return (el.innerText||el.textContent||'').trim(); })"
            ).arg(sel),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── fetch.attr ────────────────────────────────────────────────────────────
    if (c == "fetch.attr") {
        QString sel  = payload["selector"].toString();
        QString attr = payload["attr"].toString();
        sel.replace("'", "\\'"); attr.replace("'", "\\'");
        p->runJavaScript(
            QString(
                "(function(){"
                "var el=document.querySelector('%1');"
                "return el ? el.getAttribute('%2') : null;"
                "})()"
            ).arg(sel, attr),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── fetch.attrAll ─────────────────────────────────────────────────────────
    if (c == "fetch.attrAll") {
        QString sel  = payload["selector"].toString();
        QString attr = payload["attr"].toString();
        sel.replace("'", "\\'"); attr.replace("'", "\\'");
        p->runJavaScript(
            QString(
                "Array.from(document.querySelectorAll('%1'))"
                ".map(function(el){ return el.getAttribute('%2'); })"
                ".filter(function(v){ return v !== null; })"
            ).arg(sel, attr),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    return false;
}