#include "PiggyServer.h"
#include "Sessionmanager.h"
#include "../NetworkCapture.h"
#include "../Interceptor.h"
#include "../../tabs/PiggyTab.h"
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QJsonDocument>
#include <QJsonArray>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

static void applyInterceptRules(Interceptor * /*interceptor*/,
                                 const QVector<InterceptRule> & /*rules*/) {}

bool piggy_handleExport(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId) {

    // ── DOM fetch ─────────────────────────────────────────────────────────────
    if (c == "search.css") {
        piggy_page(srv, tabId)->runJavaScript(PiggyTab::domExtractorJS(),
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }
    if (c == "search.id") {
        QString js = QString(
            "(function(){ var el=document.getElementById('%1');"
            "if(!el) return null;"
            "return {tag:el.tagName.toLowerCase(),id:el.id,cls:el.className,"
            "text:el.innerText.slice(0,200),html:el.innerHTML.slice(0,500)}; })()"
        ).arg(payload["query"].toString());
        piggy_page(srv, tabId)->runJavaScript(js,
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }
    if (c == "fetch.text") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "return el?el.innerText.trim():null; })()"
        ).arg(payload["query"].toString());
        piggy_page(srv, tabId)->runJavaScript(js,
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }
    if (c == "fetch.links") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('a')).map(a=>a.href).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        piggy_page(srv, tabId)->runJavaScript(js,
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }
    if (c == "fetch.links.all") {
        piggy_page(srv, tabId)->runJavaScript(
            "(function(){ return Array.from(document.querySelectorAll('a'))"
            ".map(a=>a.href).filter(Boolean); })()",
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }
    if (c == "fetch.image") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('img')).map(i=>i.src).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        piggy_page(srv, tabId)->runJavaScript(js,
            [srv, client, id](const QVariant &r) { srv->respond(client, id, true, r); });
        return true;
    }

    // ── Cookie commands ───────────────────────────────────────────────────────
    if (c == "cookie.set") {
        QString name   = payload["name"].toString();
        QString value  = payload["value"].toString();
        QString domain = payload["domain"].toString();
        QString path   = payload["path"].toString("/");
        bool httpOnly  = payload["httpOnly"].toBool(false);
        bool secure    = payload["secure"].toBool(false);
        qint64 expiry  = payload["expiry"].toVariant().toLongLong();

        if (name.isEmpty() || domain.isEmpty()) {
            srv->respond(client, id, false, "cookie.set requires name and domain");
            return true;
        }

        QNetworkCookie cookie;
        cookie.setName(name.toUtf8());
        cookie.setValue(value.toUtf8());
        cookie.setDomain(domain);
        cookie.setPath(path);
        cookie.setHttpOnly(httpOnly);
        cookie.setSecure(secure);
        if (expiry > 0)
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(expiry));

        QString host = domain.startsWith('.') ? domain.mid(1) : domain;
        piggy_page(srv, tabId)->profile()->cookieStore()->setCookie(
            cookie, QUrl((secure ? "https" : "http") + QString("://") + host));

        if (srv->session()) srv->session()->saveCookieToFile(cookie);
        srv->respond(client, id, true, "cookie set");
        return true;
    }

    if (c == "cookie.get") {
        srv->respond(client, id, false, "cookie.get not implemented (async)");
        return true;
    }

    if (c == "cookie.delete") {
        QString name   = payload["name"].toString();
        QString domain = payload["domain"].toString();
        if (name.isEmpty() || domain.isEmpty()) {
            srv->respond(client, id, false, "cookie.delete requires name and domain");
            return true;
        }
        QNetworkCookie cookie;
        cookie.setName(name.toUtf8());
        cookie.setDomain(domain);
        QString host = domain.startsWith('.') ? domain.mid(1) : domain;
        piggy_page(srv, tabId)->profile()->cookieStore()->deleteCookie(
            cookie, QUrl("https://" + host));
        if (srv->session()) srv->session()->removeCookieFromFile(name, domain);
        srv->respond(client, id, true, "cookie deleted");
        return true;
    }

    if (c == "cookie.list") {
        srv->respond(client, id, false, "cookie.list not implemented");
        return true;
    }

    // ── Session commands ──────────────────────────────────────────────────────

    if (c == "session.reload") {
        if (srv->session()) {
            srv->session()->load();
            srv->respond(client, id, true, "session reloaded from disk");
        } else {
            srv->respond(client, id, false, "no session manager");
        }
        return true;
    }

    if (c == "session.cookies.path") {
        srv->respond(client, id, true, SessionManager::cookiesPath());
        return true;
    }

    if (c == "session.profile.path") {
        srv->respond(client, id, true, SessionManager::profilePath());
        return true;
    }

    if (c == "session.ws.path") {
        srv->respond(client, id, true, SessionManager::wsPath());
        return true;
    }

    if (c == "session.pings.path") {
        srv->respond(client, id, true, SessionManager::pingsPath());
        return true;
    }

    // Return all data file paths at once — useful for the JS library to show
    // the user where everything is on startup.
    if (c == "session.paths") {
        QJsonObject paths;
        paths["workDir"]  = SessionManager::workDir();
        paths["cookies"]  = SessionManager::cookiesPath();
        paths["profile"]  = SessionManager::profilePath();
        paths["ws"]       = SessionManager::wsPath();
        paths["pings"]    = SessionManager::pingsPath();
        srv->respond(client, id, true, paths);
        return true;
    }

    // Enable / disable ws.json persistence (opt-in)
    if (c == "session.ws.save") {
        bool on = payload["enabled"].toBool(true);
        if (srv->session()) srv->session()->setSaveWs(on);
        srv->respond(client, id, true, on ? "ws saving enabled" : "ws saving disabled");
        return true;
    }

    // Enable / disable pings.json persistence (opt-in)
    if (c == "session.pings.save") {
        bool on = payload["enabled"].toBool(true);
        if (srv->session()) srv->session()->setSavePings(on);
        srv->respond(client, id, true, on ? "pings saving enabled" : "pings saving disabled");
        return true;
    }

    // ── Intercept rules ───────────────────────────────────────────────────────
    if (c == "intercept.rule.add") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        InterceptRule rule;
        rule.urlPattern  = payload["pattern"].toString();
        rule.block       = payload["block"].toBool(false);
        rule.redirectUrl = payload["redirect"].toString();
        QJsonObject hdrs = payload["setHeaders"].toObject();
        for (auto it = hdrs.begin(); it != hdrs.end(); ++it)
            rule.setHeaders[it.key()] = it.value().toString();
        QJsonArray rem = payload["removeHeaders"].toArray();
        for (auto v : rem) rule.removeHeaders[v.toString()] = "";
        srv->tabs()[tabId].rules.append(rule);
        applyInterceptRules(srv->tabs()[tabId].interceptor, srv->tabs()[tabId].rules);
        srv->respond(client, id, true, "rule added");
        return true;
    }
    if (c == "intercept.rule.clear") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        srv->tabs()[tabId].rules.clear();
        applyInterceptRules(srv->tabs()[tabId].interceptor, {});
        srv->respond(client, id, true, "rules cleared");
        return true;
    }

    // ── Session export / import ───────────────────────────────────────────────
    if (c == "session.export") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QJsonObject root;
        root["url"] = piggy_page(srv, tabId)->url().toString();
        QJsonArray reqArr;
        for (const auto &r : srv->tabs()[tabId].capturedRequests) {
            QJsonObject o;
            o["method"] = r.method; o["url"]    = r.url;
            o["status"] = r.status; o["type"]   = r.type;
            o["mime"]   = r.mimeType;
            o["reqHeaders"] = r.requestHeaders; o["reqBody"]    = r.requestBody;
            o["resHeaders"] = r.responseHeaders; o["resBody"]   = r.responseBody;
            reqArr.append(o);
        }
        root["requests"] = reqArr;
        QJsonArray wsArr;
        for (const auto &f : srv->tabs()[tabId].capturedWsFrames) {
            QJsonObject o;
            o["url"] = f.url; o["direction"] = f.direction;
            o["data"] = f.data; o["binary"] = f.isBinary;
            wsArr.append(o);
        }
        root["ws"] = wsArr;
        QJsonArray ckArr;
        for (const auto &ck : srv->tabs()[tabId].capturedCookies) {
            QJsonObject o;
            o["name"] = ck.name; o["value"] = ck.value; o["domain"] = ck.domain;
            ckArr.append(o);
        }
        root["cookies"] = ckArr;
        srv->respond(client, id, true,
                     QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
        return true;
    }

    if (c == "session.import") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QJsonDocument doc = QJsonDocument::fromJson(payload["data"].toString().toUtf8());
        if (!doc.isObject()) { srv->respond(client, id, false, "invalid JSON data"); return true; }
        QJsonObject root = doc.object();
        TabContext &ctx  = srv->tabs()[tabId];
        for (auto v : root["requests"].toArray()) {
            CapturedRequest req; QJsonObject o = v.toObject();
            req.method = o["method"].toString(); req.url    = o["url"].toString();
            req.status = o["status"].toString(); req.type   = o["type"].toString();
            req.mimeType        = o["mime"].toString();
            req.requestHeaders  = o["reqHeaders"].toString();
            req.requestBody     = o["reqBody"].toString();
            req.responseHeaders = o["resHeaders"].toString();
            req.responseBody    = o["resBody"].toString();
            ctx.capturedRequests.append(req);
        }
        for (auto v : root["ws"].toArray()) {
            WebSocketFrame f; QJsonObject o = v.toObject();
            f.url = o["url"].toString(); f.direction = o["direction"].toString();
            f.data = o["data"].toString(); f.isBinary = o["binary"].toBool();
            ctx.capturedWsFrames.append(f);
        }
        for (auto v : root["cookies"].toArray()) {
            CapturedCookie ck; QJsonObject o = v.toObject();
            ck.name = o["name"].toString(); ck.value = o["value"].toString();
            ck.domain = o["domain"].toString();
            ctx.capturedCookies.append(ck);
        }
        srv->respond(client, id, true, "session imported");
        return true;
    }

    // ── expose.function ───────────────────────────────────────────────────────
    if (c == "expose.function") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QString fnName = payload["name"].toString();
        if (fnName.isEmpty()) { srv->respond(client, id, false, "name required"); return true; }
        TabContext &ctx = srv->tabs()[tabId];
        if (!ctx.exposedFunctions.contains(fnName))
            ctx.exposedFunctions.append(fnName);
        auto *p = piggy_page(srv, tabId);
        p->runJavaScript(NetworkCapture::exposeFunctionScript(fnName));
        QWebEngineScript script;
        script.setName("nothing_expose_" + tabId + "_" + fnName);
        script.setSourceCode(NetworkCapture::exposeFunctionScript(fnName));
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setRunsOnSubFrames(true);
        p->profile()->scripts()->insert(script);
        if (!ctx.exposedConnected) {
            QObject::connect(ctx.capture, &NetworkCapture::exposedFunctionCalled, srv,
                [srv, tabId](const QString &name, const QString &callId, const QString &data) {
                    srv->onExposedFunctionCalled(name, callId, data, tabId);
                });
            ctx.exposedConnected = true;
        }
        srv->respond(client, id, true, "exposed: " + fnName);
        return true;
    }

    if (c == "exposed.result") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QString callId  = payload["callId"].toString();
        QString result  = payload["result"].toString();
        bool    isError = payload["isError"].toBool(false);
        QString js = QString(
            "if (window.__NOTHING_RESOLVE_EXPOSED__) {"
            "  window.__NOTHING_RESOLVE_EXPOSED__('%1', '%2', %3);"
            "}"
        ).arg(callId,
              result.replace("\\", "\\\\").replace("'", "\\'"),
              isError ? "true" : "false");
        piggy_page(srv, tabId)->runJavaScript(js);
        srv->respond(client, id, true, "resolved");
        return true;
    }

    // ── addInitScript ─────────────────────────────────────────────────────────
    if (c == "addInitScript") {
        if (!srv->tabs().contains(tabId)) { srv->respond(client, id, false, "invalid tabId"); return true; }
        QString js = payload["js"].toString();
        if (js.isEmpty()) { srv->respond(client, id, false, "js required"); return true; }
        TabContext &ctx = srv->tabs()[tabId];
        ctx.initScripts.append(js);
        QWebEngineScript script;
        QString scriptName = QString("nothing_init_%1_%2").arg(tabId).arg(ctx.initScripts.size());
        script.setName(scriptName);
        script.setSourceCode(js);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setRunsOnSubFrames(true);
        piggy_page(srv, tabId)->profile()->scripts()->insert(script);
        srv->respond(client, id, true, "init script added");
        return true;
    }

    return false;
}