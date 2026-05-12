#include "PiggyFind.h"
#include "PiggyServer.h"
#include <QWebEnginePage>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── shared element serialiser (injected inline) ──────────────────────────────
// Returns a JS function string __nb_serialize(el) → plain object.
static const QString kSerialize = R"JS(
function __nb_serialize(el) {
    if (!el) return null;
    var attrs = {};
    for (var i = 0; i < el.attributes.length; i++) {
        attrs[el.attributes[i].name] = el.attributes[i].value;
    }
    return {
        tag:   el.tagName ? el.tagName.toLowerCase() : '',
        id:    el.id   || '',
        cls:   el.className || '',
        text:  (el.innerText  || '').slice(0, 400),
        html:  (el.innerHTML  || '').slice(0, 800),
        href:  el.href  || '',
        src:   el.src   || '',
        value: el.value !== undefined ? String(el.value) : '',
        attrs: attrs
    };
}
function __nb_serializeAll(list) {
    var out = [];
    for (var i = 0; i < list.length; i++) out.push(__nb_serialize(list[i]));
    return out;
}
)JS";

// ─── helper: build + run JS that returns array of element descriptors ─────────
static void runFindJs(PiggyServer *srv, QLocalSocket *client,
                      const QString &id, const QString &tabId,
                      const QString &queryExpr)           // JS expr → NodeList / Element[]
{
    QString js = QString(
        "(function(){"
        "%1"                        // serialize helpers
        "var results = %2;"         // query expr → NodeList or array or single Element
        "if (!results) return [];"
        "if (results.nodeType) return [__nb_serialize(results)];" // single element
        "return __nb_serializeAll(Array.from(results));"
        "})()"
    ).arg(kSerialize, queryExpr);

    piggy_page(srv, tabId)->runJavaScript(js,
        [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r);
        });
}

// ─── command router ───────────────────────────────────────────────────────────

bool piggy_handleFind(PiggyServer *srv, const QString &c,
                      const QJsonObject &payload,
                      QLocalSocket *client, const QString &id,
                      const QString &tabId)
{
    if (!c.startsWith("find.")) return false;

    auto *p = piggy_page(srv, tabId);

    // ── find.css ──────────────────────────────────────────────────────────────
    if (c == "find.css") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString("document.querySelectorAll('%1')").arg(sel));
        return true;
    }

    // ── find.first ────────────────────────────────────────────────────────────
    if (c == "find.first") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString("document.querySelector('%1')").arg(sel));
        return true;
    }

    // ── find.all (alias for find.css) ─────────────────────────────────────────
    if (c == "find.all") {
        QString sel = payload["selector"].toString();
        sel.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString("document.querySelectorAll('%1')").arg(sel));
        return true;
    }

    // ── find.byText ───────────────────────────────────────────────────────────
    if (c == "find.byText") {
        QString text     = payload["text"].toString();
        QString selector = payload["selector"].toString();
        bool exact       = payload["exact"].toBool(false);
        text.replace("\\", "\\\\").replace("'", "\\'");
        selector.replace("'", "\\'");

        QString scope  = selector.isEmpty() ? "document" : QString("document.querySelector('%1')").arg(selector);
        QString cmpFn  = exact
            ? QString("function match(el){ return (el.innerText||'').trim() === '%1'; }").arg(text)
            : QString("function match(el){ return (el.innerText||'').toLowerCase().indexOf('%1') !== -1; }").arg(text.toLower());

        runFindJs(srv, client, id, tabId,
            QString(
                "(function(){"
                "var scope = %1;"
                "if(!scope) return [];"
                "%2"
                "return Array.from(scope.querySelectorAll('*')).filter(match);"
                "})()"
            ).arg(scope, cmpFn));
        return true;
    }

    // ── find.byAttr ───────────────────────────────────────────────────────────
    if (c == "find.byAttr") {
        QString attr     = payload["attr"].toString();
        QString value    = payload["value"].toString();
        QString selector = payload["selector"].toString();
        attr.replace("'", "\\'"); value.replace("'", "\\'"); selector.replace("'", "\\'");

        QString attrSel = value.isEmpty()
            ? QString("[%1]").arg(attr)
            : QString("[%1='%2']").arg(attr, value);
        if (!selector.isEmpty()) attrSel = selector + attrSel;

        runFindJs(srv, client, id, tabId,
            QString("document.querySelectorAll('%1')").arg(attrSel));
        return true;
    }

    // ── find.byTag ────────────────────────────────────────────────────────────
    if (c == "find.byTag") {
        QString tag = payload["tag"].toString();
        tag.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString("document.getElementsByTagName('%1')").arg(tag));
        return true;
    }

    // ── find.byPlaceholder ────────────────────────────────────────────────────
    if (c == "find.byPlaceholder") {
        QString text = payload["text"].toString();
        text.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString(
                "Array.from(document.querySelectorAll('[placeholder]'))"
                ".filter(function(el){ return (el.getAttribute('placeholder')||'')"
                ".toLowerCase().indexOf('%1') !== -1; })"
            ).arg(text.toLower()));
        return true;
    }

    // ── find.byRole ───────────────────────────────────────────────────────────
    if (c == "find.byRole") {
        QString role = payload["role"].toString();
        QString name = payload["name"].toString();
        role.replace("'", "\\'"); name.replace("'", "\\'");

        QString filter = name.isEmpty()
            ? ""
            : QString(".filter(function(el){ return (el.getAttribute('aria-label')||el.innerText||'')"
                      ".toLowerCase().indexOf('%1') !== -1; })").arg(name.toLower());

        runFindJs(srv, client, id, tabId,
            QString(
                "Array.from(document.querySelectorAll('[role=\"%1\"]'))%2"
            ).arg(role, filter));
        return true;
    }

    // ── find.closest ─────────────────────────────────────────────────────────
    if (c == "find.closest") {
        QString selector = payload["selector"].toString();
        QString ancestor = payload["ancestor"].toString();
        selector.replace("'", "\\'"); ancestor.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString(
                "(function(){"
                "var el = document.querySelector('%1');"
                "return el ? el.closest('%2') : null;"
                "})()"
            ).arg(selector, ancestor));
        return true;
    }

    // ── find.parent ───────────────────────────────────────────────────────────
    if (c == "find.parent") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString(
                "(function(){"
                "var el = document.querySelector('%1');"
                "return el ? el.parentElement : null;"
                "})()"
            ).arg(selector));
        return true;
    }

    // ── find.children ─────────────────────────────────────────────────────────
    if (c == "find.children") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString(
                "(function(){"
                "var el = document.querySelector('%1');"
                "return el ? Array.from(el.children) : [];"
                "})()"
            ).arg(selector));
        return true;
    }

    // ── find.filter ───────────────────────────────────────────────────────────
    if (c == "find.filter") {
        QString selector = payload["selector"].toString();
        QString attr     = payload["attr"].toString();
        QString value    = payload["value"].toString();
        selector.replace("'", "\\'"); attr.replace("'", "\\'"); value.replace("'", "\\'");
        runFindJs(srv, client, id, tabId,
            QString(
                "Array.from(document.querySelectorAll('%1'))"
                ".filter(function(el){"
                "  return (el.getAttribute('%2')||'').indexOf('%3') !== -1;"
                "})"
            ).arg(selector, attr, value));
        return true;
    }

    // ── find.count ────────────────────────────────────────────────────────────
    if (c == "find.count") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        p->runJavaScript(
            QString("document.querySelectorAll('%1').length").arg(selector),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── find.exists ───────────────────────────────────────────────────────────
    if (c == "find.exists") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        p->runJavaScript(
            QString("!!document.querySelector('%1')").arg(selector),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── find.visible ─────────────────────────────────────────────────────────
    if (c == "find.visible") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        p->runJavaScript(
            QString(
                "(function(){"
                "var el=document.querySelector('%1');"
                "if(!el) return false;"
                "var s=window.getComputedStyle(el);"
                "return s.display!=='none' && s.visibility!=='hidden' && s.opacity!=='0';"
                "})()"
            ).arg(selector),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── find.enabled ─────────────────────────────────────────────────────────
    if (c == "find.enabled") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        p->runJavaScript(
            QString(
                "(function(){"
                "var el=document.querySelector('%1');"
                "return el ? !el.disabled : false;"
                "})()"
            ).arg(selector),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    // ── find.checked ─────────────────────────────────────────────────────────
    if (c == "find.checked") {
        QString selector = payload["selector"].toString();
        selector.replace("'", "\\'");
        p->runJavaScript(
            QString(
                "(function(){"
                "var el=document.querySelector('%1');"
                "return el ? !!el.checked : false;"
                "})()"
            ).arg(selector),
            [srv, client, id](const QVariant &r) {
                srv->respond(client, id, true, r);
            });
        return true;
    }

    return false;
}