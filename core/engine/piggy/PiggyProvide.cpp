#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QJsonObject>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── provide.* — actual data extraction ────────────────────────────────────────
// Every command here returns real content (text/html/arrays/objects), never
// a plain boolean. Boolean existence/state checks belong in PiggyFind.cpp.
//
// All commands route through runJsAndRespond(), which has the page
// JSON.stringify its own result before it crosses into QVariant territory.
// This avoids the QJsonDocument::fromVariant() serialization failures we
// hit earlier with evaluate() on nested arrays-of-objects, and it also
// surfaces real JS exceptions instead of silent failures.

static QString esc(QString s) {
    return s.replace("\\", "\\\\").replace("'", "\\'");
}

// Shared JS preamble: defines __elData(el), a helper that turns a DOM
// element into a plain data object. Reused by every command that needs to
// describe an element rather than just its text.
static QString wrapJs(const QString &body) {
    return QString(
        "(function(){"
        "function __elData(el){"
        "  if (!el) return null;"
        "  var attrs = {};"
        "  for (var i = 0; i < el.attributes.length; i++) {"
        "    attrs[el.attributes[i].name] = el.attributes[i].value;"
        "  }"
        "  return {"
        "    tag:  el.tagName ? el.tagName.toLowerCase() : '',"
        "    id:   el.id || '',"
        "    cls:  el.className || '',"
        "    text: (el.innerText || el.textContent || '').slice(0, 1000),"
        "    html: (el.innerHTML || '').slice(0, 2000),"
        "    attrs: attrs"
        "  };"
        "}"
        "try {"
        "  var __r = (function(){ %1 })();"
        "  return JSON.stringify({ __piggyOk: true, value: (__r === undefined ? null : __r) });"
        "} catch (e) {"
        "  return JSON.stringify({ __piggyOk: false, error: String(e.message || e) });"
        "}"
        "})()"
    ).arg(body);
}

// Runs wrapped JS, parses the {__piggyOk, value|error} envelope, and
// responds accordingly. This is the one place that talks to
// PiggyServer::respond() for the whole file.
static void runJsAndRespond(QWebEnginePage *p, PiggyServer *srv,
                             QLocalSocket *client, const QString &id,
                             const QString &body) {
    p->runJavaScript(wrapJs(body), [srv, client, id](const QVariant &r) {
        QJsonDocument doc = QJsonDocument::fromJson(r.toString().toUtf8());
        if (doc.isNull() || !doc.isObject()) {
            srv->respond(client, id, false, "invalid JS result");
            return;
        }
        QJsonObject obj = doc.object();
        if (!obj["__piggyOk"].toBool()) {
            srv->respond(client, id, false, obj["error"].toString());
            return;
        }
        srv->respond(client, id, true, obj["value"].toVariant());
    });
}

bool piggy_handleProvide(PiggyServer *srv, const QString &c,
                          const QJsonObject &payload,
                          QLocalSocket *client, const QString &id,
                          const QString &tabId) {
    auto *p = piggy_page(srv, tabId);

    // ── provide.text — single element's text ───────────────────────────────────
    if (c == "provide.text") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "return el ? el.innerText.trim() : null;"
        ).arg(sel));
        return true;
    }

    // ── provide.textAll — all matching elements' text ──────────────────────────
    if (c == "provide.textAll") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('%1'))"
            "  .map(function(el){ return el.innerText.trim(); });"
        ).arg(sel));
        return true;
    }

    // ── provide.attr — one attribute from the first match ──────────────────────
    if (c == "provide.attr") {
        QString sel  = esc(payload["selector"].toString());
        QString attr = esc(payload["attr"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "return el ? el.getAttribute('%2') : null;"
        ).arg(sel, attr));
        return true;
    }

    // ── provide.attrAll — one attribute across all matches ──────────────────────
    if (c == "provide.attrAll") {
        QString sel  = esc(payload["selector"].toString());
        QString attr = esc(payload["attr"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('%1'))"
            "  .map(function(el){ return el.getAttribute('%2'); });"
        ).arg(sel, attr));
        return true;
    }

    // ── provide.html — outerHTML of the first match (opts.inner=true → innerHTML) ─
    if (c == "provide.html") {
        QString sel   = esc(payload["selector"].toString());
        bool inner    = payload["inner"].toBool(false);
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "if (!el) return null;"
            "return %2;"
        ).arg(sel, inner ? "el.innerHTML" : "el.outerHTML"));
        return true;
    }

    // ── provide.table — parses an HTML table into rows of cell text ────────────
    if (c == "provide.table") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var table = document.querySelector('%1');"
            "if (!table) return null;"
            "return Array.from(table.querySelectorAll('tr')).map(function(tr){"
            "  return Array.from(tr.querySelectorAll('th,td')).map(function(cell){"
            "    return cell.innerText.trim();"
            "  });"
            "});"
        ).arg(sel));
        return true;
    }

    // ── provide.list — <li> text from a list container ──────────────────────────
    if (c == "provide.list") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var root = document.querySelector('%1');"
            "if (!root) return [];"
            "return Array.from(root.querySelectorAll('li')).map(function(li){"
            "  return li.innerText.trim();"
            "});"
        ).arg(sel));
        return true;
    }

    // ── provide.links — hrefs, optionally scoped to opts.selector ───────────────
    if (c == "provide.links") {
        QString sel = payload.contains("selector") ? esc(payload["selector"].toString()) : QString();
        QString scope = sel.isEmpty() ? "document" : QString("document.querySelector('%1')").arg(sel);
        runJsAndRespond(p, srv, client, id, QString(
            "var root = %1;"
            "if (!root) return [];"
            "return Array.from(root.querySelectorAll('a'))"
            "  .map(function(a){ return a.href; }).filter(Boolean);"
        ).arg(scope));
        return true;
    }

    // ── provide.images — srcs, optionally scoped to opts.selector ───────────────
    if (c == "provide.images") {
        QString sel = payload.contains("selector") ? esc(payload["selector"].toString()) : QString();
        QString scope = sel.isEmpty() ? "document" : QString("document.querySelector('%1')").arg(sel);
        runJsAndRespond(p, srv, client, id, QString(
            "var root = %1;"
            "if (!root) return [];"
            "return Array.from(root.querySelectorAll('img'))"
            "  .map(function(i){ return i.src; }).filter(Boolean);"
        ).arg(scope));
        return true;
    }

    // ── provide.form — serializes a form's fields into {name: value} ────────────
    if (c == "provide.form") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var form = document.querySelector('%1');"
            "if (!form) return null;"
            "var out = {};"
            "Array.from(form.elements).forEach(function(el){"
            "  if (!el.name) return;"
            "  if (el.type === 'checkbox' || el.type === 'radio') {"
            "    out[el.name] = el.checked;"
            "  } else {"
            "    out[el.name] = el.value;"
            "  }"
            "});"
            "return out;"
        ).arg(sel));
        return true;
    }

    // ── provide.page — full outer HTML of the document ──────────────────────────
    if (c == "provide.page") {
        runJsAndRespond(p, srv, client, id,
            "return document.documentElement.outerHTML;");
        return true;
    }

    // ── provide.div — text + html of a single element (div or otherwise) ────────
    if (c == "provide.div") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "return __elData(el);"
        ).arg(sel));
        return true;
    }

    // ── provide.meta — all <meta> tags as {name: content} ───────────────────────
    if (c == "provide.meta") {
        runJsAndRespond(p, srv, client, id,
            "var out = {};"
            "Array.from(document.querySelectorAll('meta')).forEach(function(m){"
            "  var key = m.getAttribute('name') || m.getAttribute('property');"
            "  if (key) out[key] = m.getAttribute('content');"
            "});"
            "return out;");
        return true;
    }

    // ── provide.select — options + currently selected value of a <select> ───────
    if (c == "provide.select") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "if (!el) return null;"
            "return {"
            "  value: el.value,"
            "  options: Array.from(el.options).map(function(o){"
            "    return { value: o.value, text: o.text, selected: o.selected };"
            "  })"
            "};"
        ).arg(sel));
        return true;
    }

    // ── provide.json — parses an element's textContent as JSON ──────────────────
    if (c == "provide.json") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "if (!el) return null;"
            "return JSON.parse(el.textContent);"
        ).arg(sel));
        return true;
    }

    // ── provide.count — actual number of matches (not a boolean) ────────────────
    if (c == "provide.count") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return document.querySelectorAll('%1').length;"
        ).arg(sel));
        return true;
    }

    // ── provide.first — data of the first matching element ──────────────────────
    if (c == "provide.first") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "return __elData(el);"
        ).arg(sel));
        return true;
    }

    // ── provide.all — data of every matching element ────────────────────────────
    if (c == "provide.all") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('%1')).map(__elData);"
        ).arg(sel));
        return true;
    }

    // ── provide.closest — closest ancestor of opts.selector matching opts.ancestorSelector ─
    if (c == "provide.closest") {
        QString sel      = esc(payload["selector"].toString());
        QString ancestor = esc(payload["ancestorSelector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "if (!el) return null;"
            "return __elData(el.closest('%2'));"
        ).arg(sel, ancestor));
        return true;
    }

    // ── provide.parent — data of an element's parent ─────────────────────────────
    if (c == "provide.parent") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "if (!el) return null;"
            "return __elData(el.parentElement);"
        ).arg(sel));
        return true;
    }

    // ── provide.children — data of an element's direct children ─────────────────
    if (c == "provide.children") {
        QString sel = esc(payload["selector"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "var el = document.querySelector('%1');"
            "if (!el) return [];"
            "return Array.from(el.children).map(__elData);"
        ).arg(sel));
        return true;
    }

    // ── provide.filter — elements matching opts.selector AND opts.filter ────────
    if (c == "provide.filter") {
        QString sel    = esc(payload["selector"].toString());
        QString filter = esc(payload["filter"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('%1'))"
            "  .filter(function(el){ return el.matches('%2'); })"
            "  .map(__elData);"
        ).arg(sel, filter));
        return true;
    }

    // ── provide.byRole — elements with a matching role attribute ─────────────────
    if (c == "provide.byRole") {
        QString role = esc(payload["role"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('[role=\"%1\"]')).map(__elData);"
        ).arg(role));
        return true;
    }

    // ── provide.byTag — elements by tag name ─────────────────────────────────────
    if (c == "provide.byTag") {
        QString tag = esc(payload["tag"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.getElementsByTagName('%1')).map(__elData);"
        ).arg(tag));
        return true;
    }

    // ── provide.byPlaceholder — inputs matching a placeholder ────────────────────
    if (c == "provide.byPlaceholder") {
        QString text = esc(payload["text"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('[placeholder=\"%1\"]')).map(__elData);"
        ).arg(text));
        return true;
    }

    // ── provide.byAttr — elements matching opts.attr = opts.value ────────────────
    if (c == "provide.byAttr") {
        QString attr  = esc(payload["attr"].toString());
        QString value = esc(payload["value"].toString());
        runJsAndRespond(p, srv, client, id, QString(
            "return Array.from(document.querySelectorAll('[%1=\"%2\"]')).map(__elData);"
        ).arg(attr, value));
        return true;
    }

    return false;
}