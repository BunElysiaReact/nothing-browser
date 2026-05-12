#include "PiggyHuman.h"
#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QTimer>
#include <QRandomGenerator>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Global profile ───────────────────────────────────────────────────────────
static HumanProfile s_profile;
HumanProfile &piggy_humanProfile() { return s_profile; }

// ─── Human type ───────────────────────────────────────────────────────────────
// Types one character at a time with per-char delay + small jitter.
static void humanType(PiggyServer *srv, QLocalSocket *client, const QString &id,
                      QWebEnginePage *p, const QString &selector,
                      const QString &text, bool clear, int delayMs)
{
    // Step 1: optionally clear the field
    auto doType = [srv, client, id, p, selector, text, delayMs]() {
        if (text.isEmpty()) {
            srv->respond(client, id, true, "typed (empty)");
            return;
        }

        // We build a sequence of delayed runJavaScript calls, one per character.
        // Each char event simulates: keydown → keypress → input → keyup.
        struct TypeCtx {
            int index = 0;
            QString text;
            QString selector;
            int delayMs = 120;
            bool done = false;
        };
        auto *ctx = new TypeCtx();
        ctx->text     = text;
        ctx->selector = selector;
        ctx->selector.replace("'", "\\'");
        ctx->delayMs  = delayMs;

        // First: focus and place cursor at end
        QString focusJs = QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.focus();"
            "if(el.setSelectionRange) el.setSelectionRange(el.value.length, el.value.length);"
            "return true;"
            "})()"
        ).arg(ctx->selector);

        p->runJavaScript(focusJs, [srv, client, id, p, ctx](const QVariant &focused) {
            if (!focused.toBool()) {
                delete ctx;
                srv->respond(client, id, false, "human.type: element not found");
                return;
            }

            // Recursive char sender via QTimer
            std::function<void()> sendNext;
            sendNext = [srv, client, id, p, ctx, sendNext = std::function<void()>()]() mutable {
                if (ctx->done) return;
                if (ctx->index >= ctx->text.length()) {
                    ctx->done = true;
                    delete ctx;
                    srv->respond(client, id, true, "typed");
                    return;
                }

                QChar ch = ctx->text[ctx->index++];
                QString escaped = ch == '\'' ? "\\'" : (ch == '\\' ? "\\\\" : QString(ch));

                // Build key sequence for this char
                QString js = QString(
                    "(function(){"
                    "var el=document.querySelector('%1');"
                    "if(!el) return false;"
                    "var ch='%2';"
                    "el.dispatchEvent(new KeyboardEvent('keydown',  {key:ch, bubbles:true, cancelable:true}));"
                    "el.dispatchEvent(new KeyboardEvent('keypress', {key:ch, bubbles:true, cancelable:true}));"
                    "el.value += ch;"
                    "el.dispatchEvent(new Event('input',  {bubbles:true}));"
                    "el.dispatchEvent(new KeyboardEvent('keyup',    {key:ch, bubbles:true, cancelable:true}));"
                    "return true;"
                    "})()"
                ).arg(ctx->selector, escaped);

                p->runJavaScript(js, [srv, client, id, p, ctx, sendNext](const QVariant &) mutable {
                    if (ctx->done) return;
                    // Add jitter: ±30% of base delay
                    int jitter = QRandomGenerator::global()->bounded(ctx->delayMs / 3);
                    int delay  = ctx->delayMs - ctx->delayMs/6 + jitter;
                    QTimer::singleShot(delay, [sendNext]() mutable { sendNext(); });
                });
            };

            sendNext();
        });
    };

    if (clear) {
        // Ctrl+A then Delete to clear
        QString clearJs = QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "if(!el) return;"
            "el.focus();"
            "el.select();"
            "el.value='';"
            "el.dispatchEvent(new Event('input',{bubbles:true}));"
            "})()"
        ).arg(QString(selector).replace("'", "\\'"));
        p->runJavaScript(clearJs, [doType](const QVariant &) { doType(); });
    } else {
        doType();
    }
}

// ─── Human click ─────────────────────────────────────────────────────────────
static void humanClick(PiggyServer *srv, QLocalSocket *client, const QString &id,
                       QWebEnginePage *p, const QString &selector,
                       bool force, int delayMs)
{
    QString sel = selector;
    sel.replace("'", "\\'");

    QString js;
    if (force) {
        js = QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.scrollIntoView({behavior:'smooth',block:'center'});"
            "var rect=el.getBoundingClientRect();"
            "var cx=rect.left+rect.width/2, cy=rect.top+rect.height/2;"
            "['mouseover','mouseenter','mousemove','mousedown','mouseup','click'].forEach(function(t){"
            "  el.dispatchEvent(new MouseEvent(t,{clientX:cx,clientY:cy,bubbles:true,cancelable:true}));"
            "});"
            "return true;"
            "})()"
        ).arg(sel);
    } else {
        js = QString(
            "(function(){"
            "var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.scrollIntoView({behavior:'smooth',block:'center'});"
            "el.click();"
            "return true;"
            "})()"
        ).arg(sel);
    }

    QTimer::singleShot(delayMs, [srv, client, id, p, js]() {
        p->runJavaScript(js, [srv, client, id](const QVariant &r) {
            srv->respond(client, id, true, r);
        });
    });
}

// ─── Command router ───────────────────────────────────────────────────────────

bool piggy_handleHuman(PiggyServer *srv, const QString &c,
                       const QJsonObject &payload,
                       QLocalSocket *client, const QString &id,
                       const QString &tabId)
{
    auto *p = piggy_page(srv, tabId);

    if (c == "human.set") {
        if (payload.contains("typingSpeed")) s_profile.typingSpeed = payload["typingSpeed"].toString();
        if (payload.contains("clickDelay"))  s_profile.clickDelay  = payload["clickDelay"].toString();
        if (payload.contains("scrollSpeed")) s_profile.scrollSpeed = payload["scrollSpeed"].toString();
        if (payload.contains("mouseWiggle")) s_profile.mouseWiggle = payload["mouseWiggle"].toBool();
        QJsonObject o;
        o["typingSpeed"] = s_profile.typingSpeed;
        o["clickDelay"]  = s_profile.clickDelay;
        o["scrollSpeed"] = s_profile.scrollSpeed;
        o["mouseWiggle"] = s_profile.mouseWiggle;
        srv->respond(client, id, true, o);
        return true;
    }

    if (c == "human.get") {
        QJsonObject o;
        o["typingSpeed"] = s_profile.typingSpeed;
        o["clickDelay"]  = s_profile.clickDelay;
        o["scrollSpeed"] = s_profile.scrollSpeed;
        o["mouseWiggle"] = s_profile.mouseWiggle;
        srv->respond(client, id, true, o);
        return true;
    }

    if (c == "human.type") {
        QString sel   = payload["selector"].toString();
        QString text  = payload["text"].toString();
        bool clear    = payload["clear"].toBool(false);
        int speed     = payload["speed"].toInt(s_profile.typingDelayMs());
        humanType(srv, client, id, p, sel, text, clear, speed);
        return true;
    }

    if (c == "human.click") {
        QString sel = payload["selector"].toString();
        bool force  = payload["force"].toBool(false);
        int delay   = payload["delay"].toInt(s_profile.clickDelayMs());
        humanClick(srv, client, id, p, sel, force, delay);
        return true;
    }

    // ── type (override the existing type command with clear option) ────────────
    // Extends the basic PiggyInteractions type with { clear: true } support.
    if (c == "type") {
        bool clear = payload["clear"].toBool(false);
        if (clear) {
            QString sel  = payload["selector"].toString();
            QString text = payload["text"].toString();
            // Delegate to human type at instant speed (0ms delay)
            humanType(srv, client, id, p, sel, text, true, 0);
            return true;
        }
        return false; // let PiggyInteractions handle non-clear type
    }

    return false;
}