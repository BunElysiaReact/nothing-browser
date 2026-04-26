#include "PiggyServer.h"
#include <QWebEnginePage>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── All DOM/input interaction commands ───────────────────────────────────────

bool piggy_handleInteraction(PiggyServer *srv, const QString &c,
                              const QJsonObject &payload,
                              QLocalSocket *client, const QString &id,
                              const QString &tabId) {
    auto *p = piggy_page(srv, tabId);

    if (c == "click") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(el){el.click();return true;} return false; })()"
        ).arg(payload["selector"].toString());
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "dblclick") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('dblclick',{bubbles:true,cancelable:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString());
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "hover") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('mouseover',{bubbles:true}));"
            "el.dispatchEvent(new MouseEvent('mouseenter',{bubbles:false}));"
            "return true; })()"
        ).arg(payload["selector"].toString());
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "type") {
        QString text = payload["text"].toString();
        text.replace("\\", "\\\\").replace("'", "\\'");
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.focus(); el.value='%2';"
            "el.dispatchEvent(new Event('input',{bubbles:true}));"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString(), text);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "select") {
        QString val = payload["value"].toString();
        val.replace("\\", "\\\\").replace("'", "\\'");
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.value='%2';"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString(), val);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "scroll.to") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.scrollIntoView({behavior:'smooth',block:'center'});"
            "return true; })()"
        ).arg(payload["selector"].toString());
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "scroll.by") {
        int px = payload["px"].toInt(300);
        p->runJavaScript(
            QString("window.scrollBy({top:%1,behavior:'smooth'}); true;").arg(px),
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "keyboard.press") {
        QString key = payload["key"].toString();
        key.replace("'", "\\'");
        QString js = QString(
            "(function(){"
            "var el=document.activeElement||document.body;"
            "['keydown','keypress','keyup'].forEach(function(t){"
            "  el.dispatchEvent(new KeyboardEvent(t,{key:'%1',bubbles:true,cancelable:true}));"
            "});"
            "return true; })()"
        ).arg(key);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "keyboard.combo") {
        QString combo = payload["combo"].toString();
        QStringList parts = combo.split('+');
        QString mainKey = parts.last();
        mainKey.replace("'", "\\'");
        bool ctrl  = parts.contains("Control", Qt::CaseInsensitive);
        bool shift = parts.contains("Shift",   Qt::CaseInsensitive);
        bool alt   = parts.contains("Alt",     Qt::CaseInsensitive);
        QString js = QString(
            "(function(){"
            "var el=document.activeElement||document.body;"
            "var opts={key:'%1',ctrlKey:%2,shiftKey:%3,altKey:%4,bubbles:true,cancelable:true};"
            "['keydown','keypress','keyup'].forEach(function(t){"
            "  el.dispatchEvent(new KeyboardEvent(t,opts));"
            "});"
            "return true; })()"
        ).arg(mainKey,
              ctrl  ? "true" : "false",
              shift ? "true" : "false",
              alt   ? "true" : "false");
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "mouse.move") {
        int x = payload["x"].toInt(), y = payload["y"].toInt();
        p->runJavaScript(
            QString("document.dispatchEvent(new MouseEvent('mousemove',{clientX:%1,clientY:%2,bubbles:true})); true;")
                .arg(x).arg(y),
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "mouse.drag") {
        QJsonObject from = payload["from"].toObject(), to = payload["to"].toObject();
        int fx = from["x"].toInt(), fy = from["y"].toInt();
        int tx = to["x"].toInt(),   ty = to["y"].toInt();
        QString js = QString(
            "(function(){"
            "var el=document.elementFromPoint(%1,%2);"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('mousedown',{clientX:%1,clientY:%2,bubbles:true}));"
            "document.dispatchEvent(new MouseEvent('mousemove',{clientX:%3,clientY:%4,bubbles:true}));"
            "document.dispatchEvent(new MouseEvent('mouseup',  {clientX:%3,clientY:%4,bubbles:true}));"
            "return true; })()"
        ).arg(fx).arg(fy).arg(tx).arg(ty);
        p->runJavaScript(js, [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    if (c == "evaluate") {
        p->runJavaScript(payload["js"].toString(),
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    return false;
}