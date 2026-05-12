#include "PiggyIframe.h"
#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QTimer>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── helpers ──────────────────────────────────────────────────────────────────

// Build a JS expression that resolves to the target frame.
// payload must have one of:
//   "frameIndex": int        → window.frames[n]
//   "frameSrc":   string     → find by iframe[src*=...]
//   "frameId":    string     → find by iframe[id=...]
//   "frameName":  string     → window.frames[name]
static QString frameExpr(const QJsonObject &payload) {
    if (payload.contains("frameIndex")) {
        return QString("window.frames[%1]").arg(payload["frameIndex"].toInt());
    }
    if (payload.contains("frameName")) {
        QString name = payload["frameName"].toString();
        name.replace("'", "\\'");
        return QString("window.frames['%1']").arg(name);
    }
    if (payload.contains("frameSrc")) {
        QString src = payload["frameSrc"].toString();
        src.replace("'", "\\'");
        return QString(
            "(function(){"
            "  var iframes = Array.from(document.querySelectorAll('iframe'));"
            "  var match = iframes.find(function(f){ return f.src.indexOf('%1') !== -1; });"
            "  return match ? window.frames[Array.from(document.querySelectorAll('iframe')).indexOf(match)] : null;"
            "})()"
        ).arg(src);
    }
    if (payload.contains("frameId")) {
        QString fid = payload["frameId"].toString();
        fid.replace("'", "\\'");
        return QString(
            "(function(){"
            "  var el = document.getElementById('%1');"
            "  if (!el) return null;"
            "  var idx = Array.from(document.querySelectorAll('iframe')).indexOf(el);"
            "  return idx >= 0 ? window.frames[idx] : null;"
            "})()"
        ).arg(fid);
    }
    return "window.frames[0]"; // fallback: first iframe
}

// ─── command router ───────────────────────────────────────────────────────────

bool piggy_handleIframe(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id,
                        const QString &tabId)
{
    if (!c.startsWith("iframe.")) return false;

    auto *p = piggy_page(srv, tabId);

    // ── iframe.list ───────────────────────────────────────────────────────────
    if (c == "iframe.list") {
        p->runJavaScript(
            "(function(){"
            "return Array.from(document.querySelectorAll('iframe')).map(function(f, i){"
            "  return { index: i, src: f.src, id: f.id||'', name: f.name||'',"
            "           width: f.width, height: f.height };"
            "});"
            "})()",
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── iframe.evaluate ───────────────────────────────────────────────────────
    if (c == "iframe.evaluate") {
        QString js = payload["js"].toString();
        js.replace("\\", "\\\\").replace("`", "\\`");
        QString fe = frameExpr(payload);

        QString fullJs = QString(
            "(function(){"
            "  var frame = %1;"
            "  if (!frame || !frame.document) return { error: 'frame not accessible' };"
            "  try {"
            "    with (frame) { return eval(%2); }"
            "  } catch(e) { return { error: e.message }; }"
            "})()"
        ).arg(fe, "`" + js + "`");

        p->runJavaScript(fullJs,
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── iframe.click ──────────────────────────────────────────────────────────
    if (c == "iframe.click") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        QString fe = frameExpr(payload);

        p->runJavaScript(
            QString(
                "(function(){"
                "var frame = %1;"
                "if (!frame || !frame.document) return false;"
                "var el = frame.document.querySelector('%2');"
                "if (!el) return false;"
                "el.click(); return true;"
                "})()"
            ).arg(fe, sel),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── iframe.type ───────────────────────────────────────────────────────────
    if (c == "iframe.type") {
        QString sel  = payload["selector"].toString();
        QString text = payload["text"].toString();
        sel.replace("'", "\\'"); text.replace("\\", "\\\\").replace("'", "\\'");
        QString fe = frameExpr(payload);

        p->runJavaScript(
            QString(
                "(function(){"
                "var frame = %1;"
                "if (!frame || !frame.document) return false;"
                "var el = frame.document.querySelector('%2');"
                "if (!el) return false;"
                "el.focus(); el.value = '%3';"
                "el.dispatchEvent(new Event('input',  { bubbles: true }));"
                "el.dispatchEvent(new Event('change', { bubbles: true }));"
                "return true;"
                "})()"
            ).arg(fe, sel, text),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── iframe.text ───────────────────────────────────────────────────────────
    if (c == "iframe.text") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        QString fe = frameExpr(payload);

        p->runJavaScript(
            QString(
                "(function(){"
                "var frame = %1;"
                "if (!frame || !frame.document) return null;"
                "var el = frame.document.querySelector('%2');"
                "return el ? (el.innerText||'').trim() : null;"
                "})()"
            ).arg(fe, sel),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── iframe.html ───────────────────────────────────────────────────────────
    if (c == "iframe.html") {
        QString fe = frameExpr(payload);
        p->runJavaScript(
            QString(
                "(function(){"
                "var frame = %1;"
                "if (!frame || !frame.document) return null;"
                "return frame.document.documentElement.outerHTML.slice(0, 200000);"
                "})()"
            ).arg(fe),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── iframe.waitSel ────────────────────────────────────────────────────────
    if (c == "iframe.waitSel") {
        QString sel = payload["selector"].toString();
        int timeout = payload["timeout"].toInt(10000);
        sel.replace("'", "\\'");
        QString fe = frameExpr(payload);

        auto *elapsed = new int(0);
        auto *done    = new bool(false);
        auto *timer   = new QTimer(srv);
        timer->setInterval(100);

        QObject::connect(timer, &QTimer::timeout, srv,
            [srv, client, id, p, fe, sel, timeout, timer, elapsed, done]() {
                if (*done) return;
                *elapsed += 100;

                p->runJavaScript(
                    QString(
                        "(function(){"
                        "var frame = %1;"
                        "return frame && frame.document ? !!frame.document.querySelector('%2') : false;"
                        "})()"
                    ).arg(fe, sel),
                    [srv, client, id, timeout, timer, elapsed, done](const QVariant &r) {
                        if (*done) return;
                        if (r.toBool()) {
                            *done = true;
                            timer->stop(); timer->deleteLater(); delete elapsed; delete done;
                            srv->respond(client, id, true, "found");
                        } else if (*elapsed >= timeout) {
                            *done = true;
                            timer->stop(); timer->deleteLater(); delete elapsed; delete done;
                            srv->respond(client, id, false, "iframe.waitSel timeout");
                        }
                    });
            });
        timer->start();
        return true;
    }

    return false;
}