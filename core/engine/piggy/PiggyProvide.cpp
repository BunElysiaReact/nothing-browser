#include "PiggyProvide.h"
#include "PiggyServer.h"
#include <QWebEnginePage>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── helpers ─────────────────────────────────────────────────────────────────

static void runJs(PiggyServer *srv, QLocalSocket *client,
                  const QString &id, QWebEnginePage *p,
                  const QString &js)
{
    p->runJavaScript(js,
        [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r);
        });
}

// ─── command router ───────────────────────────────────────────────────────────

bool piggy_handleProvide(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId)
{
    if (!c.startsWith("provide.")) return false;

    auto *p = piggy_page(srv, tabId);

    // ── provide.text ──────────────────────────────────────────────────────────
    if (c == "provide.text") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "return el ? (el.innerText||el.textContent||'').trim() : null;"
            "})()"
        ).arg(sel));
        return true;
    }

    // ── provide.textAll ───────────────────────────────────────────────────────
    if (c == "provide.textAll") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "Array.from(document.querySelectorAll('%1'))"
            ".map(function(el){ return (el.innerText||el.textContent||'').trim(); })"
        ).arg(sel));
        return true;
    }

    // ── provide.attr ─────────────────────────────────────────────────────────
    if (c == "provide.attr") {
        QString sel  = payload["selector"].toString();
        QString attr = payload["attr"].toString();
        sel.replace("'", "\\'"); attr.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "return el ? el.getAttribute('%2') : null;"
            "})()"
        ).arg(sel, attr));
        return true;
    }

    // ── provide.attrAll ───────────────────────────────────────────────────────
    if (c == "provide.attrAll") {
        QString sel  = payload["selector"].toString();
        QString attr = payload["attr"].toString();
        sel.replace("'", "\\'"); attr.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "Array.from(document.querySelectorAll('%1'))"
            ".map(function(el){ return el.getAttribute('%2'); })"
            ".filter(function(v){ return v !== null; })"
        ).arg(sel, attr));
        return true;
    }

    // ── provide.html ─────────────────────────────────────────────────────────
    if (c == "provide.html") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "return el ? el.innerHTML : null;"
            "})()"
        ).arg(sel));
        return true;
    }

    // ── provide.table ─────────────────────────────────────────────────────────
    if (c == "provide.table") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var tbl = document.querySelector('%1');"
            "if (!tbl) return null;"
            "var headers = [];"
            "var rows = [];"
            "var ths = tbl.querySelectorAll('thead th, thead td, tr:first-child th');"
            "if (ths.length) {"
            "  ths.forEach(function(th){ headers.push((th.innerText||'').trim()); });"
            "}"
            "var trs = tbl.querySelectorAll('tbody tr, tr');"
            "trs.forEach(function(tr){"
            "  if (tr.querySelector('th') && !tr.querySelector('td')) return;" // skip header rows
            "  var cells = Array.from(tr.querySelectorAll('td,th'))"
            "    .map(function(td){ return (td.innerText||'').trim(); });"
            "  if (cells.length) rows.push(cells);"
            "});"
            "return { headers: headers, rows: rows };"
            "})()"
        ).arg(sel));
        return true;
    }

    // ── provide.list ─────────────────────────────────────────────────────────
    if (c == "provide.list") {
        QString sel     = payload["selector"].toString();
        QString itemSel = payload["itemSel"].toString("li");
        sel.replace("'", "\\'"); itemSel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var root = document.querySelector('%1');"
            "if (!root) return [];"
            "return Array.from(root.querySelectorAll('%2'))"
            "  .map(function(el){ return (el.innerText||'').trim(); })"
            "  .filter(function(t){ return t.length > 0; });"
            "})()"
        ).arg(sel, itemSel));
        return true;
    }

    // ── provide.links ─────────────────────────────────────────────────────────
    if (c == "provide.links") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        QString scope = sel.isEmpty() ? "document" : QString("document.querySelector('%1')").arg(sel);
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var root = %1;"
            "if (!root) return [];"
            "return Array.from(root.querySelectorAll('a'))"
            "  .map(function(a){"
            "    return { text: (a.innerText||'').trim(), href: a.href, title: a.title||'' };"
            "  })"
            "  .filter(function(l){ return l.href && l.href !== window.location.href; });"
            "})()"
        ).arg(scope));
        return true;
    }

    // ── provide.images ────────────────────────────────────────────────────────
    if (c == "provide.images") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        QString scope = sel.isEmpty() ? "document" : QString("document.querySelector('%1')").arg(sel);
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var root = %1;"
            "if (!root) return [];"
            "return Array.from(root.querySelectorAll('img'))"
            "  .map(function(img){"
            "    return { src: img.src, alt: img.alt||'', width: img.naturalWidth, height: img.naturalHeight };"
            "  })"
            "  .filter(function(i){ return !!i.src; });"
            "})()"
        ).arg(scope));
        return true;
    }

    // ── provide.form ─────────────────────────────────────────────────────────
    if (c == "provide.form") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var form = document.querySelector('%1');"
            "if (!form) return null;"
            "var result = {};"
            "var els = form.querySelectorAll('input,select,textarea');"
            "els.forEach(function(el){"
            "  var name = el.name || el.id || el.getAttribute('aria-label') || '';"
            "  if (!name) return;"
            "  if (el.type === 'checkbox' || el.type === 'radio') {"
            "    result[name] = el.checked;"
            "  } else if (el.tagName.toLowerCase() === 'select') {"
            "    var opt = el.options[el.selectedIndex];"
            "    result[name] = opt ? opt.value : '';"
            "  } else {"
            "    result[name] = el.value || '';"
            "  }"
            "});"
            "return result;"
            "})()"
        ).arg(sel));
        return true;
    }

    // ── provide.page ─────────────────────────────────────────────────────────
    if (c == "provide.page") {
        runJs(srv, client, id, p,
            "(function(){"
            "return {"
            "  title: document.title || '',"
            "  url:   window.location.href,"
            "  text:  (document.body ? document.body.innerText : '').slice(0, 50000),"
            "  html:  document.documentElement.outerHTML.slice(0, 200000)"
            "};"
            "})()");
        return true;
    }

    // ── provide.div ──────────────────────────────────────────────────────────
    if (c == "provide.div") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var el = document.querySelector('%1');"
            "if (!el) return null;"
            "function ser(e) {"
            "  var ch = [];"
            "  for (var i=0; i<Math.min(e.children.length, 20); i++)"
            "    ch.push({ tag: e.children[i].tagName.toLowerCase(),"
            "              text: (e.children[i].innerText||'').slice(0,200) });"
            "  return {"
            "    tag:      e.tagName.toLowerCase(),"
            "    id:       e.id||'',"
            "    cls:      e.className||'',"
            "    text:     (e.innerText||'').slice(0,2000),"
            "    html:     e.innerHTML.slice(0,4000),"
            "    children: ch"
            "  };"
            "}"
            "return ser(el);"
            "})()"
        ).arg(sel));
        return true;
    }

    // ── provide.meta ─────────────────────────────────────────────────────────
    if (c == "provide.meta") {
        runJs(srv, client, id, p,
            "(function(){"
            "var result = {};"
            "document.querySelectorAll('meta').forEach(function(m){"
            "  var key = m.getAttribute('name') || m.getAttribute('property') || m.getAttribute('http-equiv');"
            "  var val = m.getAttribute('content');"
            "  if (key && val) result[key] = val;"
            "});"
            "return result;"
            "})()");
        return true;
    }

    // ── provide.select ────────────────────────────────────────────────────────
    if (c == "provide.select") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runJs(srv, client, id, p, QString(
            "(function(){"
            "var el = document.querySelector('%1');"
            "if (!el || el.tagName.toLowerCase() !== 'select') return null;"
            "var options = [];"
            "for (var i=0; i<el.options.length; i++) {"
            "  options.push({"
            "    text:     el.options[i].text,"
            "    value:    el.options[i].value,"
            "    selected: el.options[i].selected"
            "  });"
            "}"
            "return { value: el.value, options: options };"
            "})()"
        ).arg(sel));
        return true;
    }

    // ── provide.json ─────────────────────────────────────────────────────────
    if (c == "provide.json") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");

        QString js = sel.isEmpty()
            // No selector: try window.__NEXT_DATA__, __NUXT__, or first script[type="application/json"]
            ? QString(
                "(function(){"
                "if (window.__NEXT_DATA__) return window.__NEXT_DATA__;"
                "if (window.__NUXT__)      return window.__NUXT__;"
                "var s = document.querySelector('script[type=\"application/json\"]');"
                "if (s) { try { return JSON.parse(s.textContent); } catch(e) {} }"
                "return null;"
                "})()")
            : QString(
                "(function(){"
                "var el = document.querySelector('%1');"
                "if (!el) return null;"
                "try { return JSON.parse(el.textContent||el.innerText||''); }"
                "catch(e) { return { error: e.message, raw: (el.textContent||'').slice(0,500) }; }"
                "})()"
            ).arg(sel);

        runJs(srv, client, id, p, js);
        return true;
    }

    return false;
}