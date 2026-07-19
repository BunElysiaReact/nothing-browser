#include "PiggyServer.h"
#include <QWebEnginePage>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── find.* — boolean existence/state checks only ─────────────────────────────
// Every command here returns true/false. If it returns actual data, it
// belongs in PiggyProvide.cpp instead — see NEXT_VERSION.md for the
// find vs. provide contract.

static QString escSel(QString s) {
    return s.replace("\\", "\\\\").replace("'", "\\'");
}

bool piggy_handleFind(PiggyServer *srv, const QString &c,
                       const QJsonObject &payload,
                       QLocalSocket *client, const QString &id,
                       const QString &tabId) {
    auto *p = piggy_page(srv, tabId);

    // ── find.exists ───────────────────────────────────────────────────────────
    if (c == "find.exists") {
        QString sel = escSel(payload["selector"].toString());
        QString js = QString(
            "(function(){ return !!document.querySelector('%1'); })()"
        ).arg(sel);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.matches — same check as exists, alias for readability when
    //    the caller conceptually means "does anything match this selector" ──
    if (c == "find.matches") {
        QString sel = escSel(payload["selector"].toString());
        QString js = QString(
            "(function(){ return document.querySelectorAll('%1').length > 0; })()"
        ).arg(sel);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.visible ──────────────────────────────────────────────────────────
    if (c == "find.visible") {
        QString sel = escSel(payload["selector"].toString());
        QString js = QString(
            "(function(){"
            "  var el = document.querySelector('%1');"
            "  if (!el) return false;"
            "  var cs = getComputedStyle(el);"
            "  if (cs.display === 'none' || cs.visibility === 'hidden' || cs.opacity === '0') return false;"
            "  var r = el.getBoundingClientRect();"
            "  return r.width > 0 && r.height > 0;"
            "})()"
        ).arg(sel);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.enabled ───────────────────────────────────────────────────────────
    if (c == "find.enabled") {
        QString sel = escSel(payload["selector"].toString());
        QString js = QString(
            "(function(){"
            "  var el = document.querySelector('%1');"
            "  if (!el) return false;"
            "  return !el.disabled;"
            "})()"
        ).arg(sel);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.checked ───────────────────────────────────────────────────────────
    if (c == "find.checked") {
        QString sel = escSel(payload["selector"].toString());
        QString js = QString(
            "(function(){"
            "  var el = document.querySelector('%1');"
            "  if (!el) return false;"
            "  return !!el.checked;"
            "})()"
        ).arg(sel);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.hasClass(selector, className) ─────────────────────────────────────
    if (c == "find.hasClass") {
        QString sel = escSel(payload["selector"].toString());
        QString cls = escSel(payload["className"].toString());
        QString js = QString(
            "(function(){"
            "  var el = document.querySelector('%1');"
            "  if (!el) return false;"
            "  return el.classList.contains('%2');"
            "})()"
        ).arg(sel, cls);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.hasAttr(selector, attr) ────────────────────────────────────────────
    if (c == "find.hasAttr") {
        QString sel  = escSel(payload["selector"].toString());
        QString attr = escSel(payload["attr"].toString());
        QString js = QString(
            "(function(){"
            "  var el = document.querySelector('%1');"
            "  if (!el) return false;"
            "  return el.hasAttribute('%2');"
            "})()"
        ).arg(sel, attr);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    // ── find.hasText(selector, text) ────────────────────────────────────────────
    // If selector is omitted, searches the whole body.
    if (c == "find.hasText") {
        QString sel  = escSel(payload["selector"].toString());
        QString text = escSel(payload["text"].toString());
        QString scope = sel.isEmpty() ? "document.body" : QString("document.querySelector('%1')").arg(sel);
        QString js = QString(
            "(function(){"
            "  var root = %1;"
            "  if (!root) return false;"
            "  return root.textContent.indexOf('%2') !== -1;"
            "})()"
        ).arg(scope, text);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r.toBool());
        });
        return true;
    }

    return false;
}