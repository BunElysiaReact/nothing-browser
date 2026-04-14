#include "PiggyServer.h"
#include "../tabs/PiggyTab.h"
#include "NetworkCapture.h"
#include "Interceptor.h"
#include "FingerprintSpoofer.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEngineCookieStore>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QPageLayout>
#include <QPageSize>
#include <QMarginsF>
#include <QTimer>
#include <QRegularExpression>

// ─── Constructors ─────────────────────────────────────────────────────────────

PiggyServer::PiggyServer(PiggyTab *piggy, QObject *parent)
    : QObject(parent), m_piggy(piggy)
{
    if (!m_piggy) {
        m_ownProfile = new QWebEngineProfile(this);
        m_ownProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_ownProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
        m_ownPage = new QWebEnginePage(m_ownProfile, this);
    }
}

PiggyServer::PiggyServer(QWebEnginePage *page, QObject *parent)
    : QObject(parent), m_piggy(nullptr), m_headfulPage(page)
{
    // page is owned externally — do not reparent
}

PiggyServer::~PiggyServer() { stop(); }

// ─── Tab management ──────────────────────────────────────────────────────────

QString PiggyServer::createTab() {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Decide which profile to use
    QWebEngineProfile *profile = nullptr;
    if (m_piggy)
        profile = m_piggy->getPage()->profile();
    else if (m_headfulPage)
        profile = m_headfulPage->profile();
    else
        profile = m_ownProfile;

    auto *p = new QWebEnginePage(profile, this);

    // Inject fingerprint spoof
    auto &spoofer = FingerprintSpoofer::instance();
    QWebEngineScript spoofScript;
    spoofScript.setName("nothing_fingerprint_" + id);
    spoofScript.setSourceCode(spoofer.injectionScript());
    spoofScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    spoofScript.setWorldId(QWebEngineScript::MainWorld);
    spoofScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(spoofScript);

    // Inject capture script
    QWebEngineScript capScript;
    capScript.setName("nothing_capture_" + id);
    capScript.setSourceCode(NetworkCapture::captureScript());
    capScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    capScript.setWorldId(QWebEngineScript::MainWorld);
    capScript.setRunsOnSubFrames(true);
    profile->scripts()->insert(capScript);

    auto *interceptor = new Interceptor(this);
    profile->setUrlRequestInterceptor(interceptor);

    auto *capture = new NetworkCapture(this);
    capture->attachToPage(p, profile);

    connect(capture, &NetworkCapture::requestCaptured, this,
            [this, id](const CapturedRequest &req) { onRequestCaptured(req, id); });
    connect(capture, &NetworkCapture::wsFrameCaptured, this,
            [this, id](const WebSocketFrame &frame) { onWsFrameCaptured(frame, id); });
    connect(capture, &NetworkCapture::cookieCaptured, this,
            [this, id](const CapturedCookie &cookie) { onCookieCaptured(cookie, id); });
    connect(capture, &NetworkCapture::cookieRemoved, this,
            [this, id](const QString &name, const QString &domain) {
                onCookieRemoved(name, domain, id);
            });
    connect(capture, &NetworkCapture::storageCaptured, this,
            [this, id](const QString &origin, const QString &key,
                       const QString &value, const QString &type) {
                onStorageCaptured(origin, key, value, type, id);
            });

    TabContext ctx;
    ctx.page        = p;
    ctx.interceptor = interceptor;
    ctx.capture     = capture;
    m_tabs.insert(id, ctx);

    emit tabCreated(id, p);
    qDebug() << "[PiggyServer] Tab created:" << id;
    return id;
}

void PiggyServer::closeTab(const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    ctx.page->deleteLater();
    ctx.interceptor->deleteLater();
    ctx.capture->deleteLater();
    m_tabs.remove(tabId);
    emit tabClosed(tabId);
    qDebug() << "[PiggyServer] Tab closed:" << tabId;
}

QWebEnginePage* PiggyServer::page(const QString &tabId) {
    if (!tabId.isEmpty() && tabId != "default") {
        auto it = m_tabs.find(tabId);
        if (it != m_tabs.end()) return it.value().page;
        qWarning() << "[PiggyServer] Unknown tabId:" << tabId << "— falling back to default";
    }
    if (m_piggy)       return m_piggy->getPage();
    if (m_headfulPage) return m_headfulPage;
    return m_ownPage;
}

// ─── Server lifecycle ────────────────────────────────────────────────────────

void PiggyServer::start() {
    QLocalServer::removeServer(SOCKET_NAME);
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection,
            this, &PiggyServer::onNewConnection);
    if (!m_server->listen(SOCKET_NAME)) {
        qWarning() << "[PiggyServer] Failed to start:" << m_server->errorString();
        return;
    }
    qDebug() << "[PiggyServer] Listening on" << SOCKET_NAME;
}

void PiggyServer::stop() {
    if (m_server) { m_server->close(); m_server = nullptr; }
    for (auto &ctx : m_tabs) {
        ctx.page->deleteLater();
        ctx.interceptor->deleteLater();
        ctx.capture->deleteLater();
    }
    m_tabs.clear();
}

void PiggyServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        auto *client = m_server->nextPendingConnection();
        m_clients.append(client);
        connect(client, &QLocalSocket::readyRead,
                this, &PiggyServer::onClientData);
        connect(client, &QLocalSocket::disconnected,
                this, &PiggyServer::onClientDisconnected);
        qDebug() << "[PiggyServer] Client connected";
    }
}

void PiggyServer::onClientDisconnected() {
    auto *client = qobject_cast<QLocalSocket*>(sender());
    if (client) { m_clients.removeAll(client); client->deleteLater(); }
}

void PiggyServer::onClientData() {
    auto *client = qobject_cast<QLocalSocket*>(sender());
    if (!client) return;
    QByteArray raw = client->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isNull() || !doc.isObject()) {
        respond(client, "", false, "Invalid JSON");
        return;
    }
    handleCommand(doc.object(), client);
}

// ─── Screenshot / PDF ────────────────────────────────────────────────────────

void PiggyServer::doScreenshot(QLocalSocket *client, const QString &id,
                                const QString &tabId) {
    auto *p = page(tabId);
    const QString js = QStringLiteral(
        "(function(){"
        "  try {"
        "    var w=Math.min(document.documentElement.scrollWidth||1280,8192);"
        "    var h=Math.min(document.documentElement.scrollHeight||800,16384);"
        "    var c=document.createElement('canvas');"
        "    c.width=w; c.height=h;"
        "    var ctx=c.getContext('2d');"
        "    ctx.fillStyle='#ffffff'; ctx.fillRect(0,0,w,h);"
        "    ctx.fillStyle='#000000'; ctx.font='16px sans-serif';"
        "    ctx.fillText('screenshot: '+document.title,20,40);"
        "    return c.toDataURL('image/png').split(',')[1];"
        "  } catch(e){ return null; }"
        "})()"
    );
    p->runJavaScript(js, [=](const QVariant &r) {
        if (r.isNull() || !r.isValid())
            respond(client, id, false, "screenshot: JS render failed");
        else
            respond(client, id, true, r.toString());
    });
}

void PiggyServer::doPdf(QLocalSocket *client, const QString &id,
                         const QString &tabId) {
    auto *p = page(tabId);
    QPageLayout layout(QPageSize(QPageSize::A4),
                       QPageLayout::Portrait,
                       QMarginsF(15,15,15,15),
                       QPageLayout::Millimeter);
    p->printToPdf([=](const QByteArray &pdfData) {
        if (pdfData.isEmpty()) {
            respond(client, id, false, "pdf: render failed");
            return;
        }
        respond(client, id, true, QString::fromLatin1(pdfData.toBase64()));
    }, layout);
}

void PiggyServer::setImageBlocking(const QString &tabId, bool block) {
    auto *p = page(tabId);
    p->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, !block);
    if (m_tabs.contains(tabId))
        m_tabs[tabId].imageBlocked = block;
}

// ─── Cookie helpers ───────────────────────────────────────────────────────────

QList<QNetworkCookie> PiggyServer::cookiesForTab(const QString &tabId) {
    Q_UNUSED(tabId);
    return {};
}

// ─── Interception rules ──────────────────────────────────────────────────────

void PiggyServer::applyInterceptRules(Interceptor *interceptor,
                                       const QVector<InterceptRule> &rules) {
    Q_UNUSED(interceptor);
    Q_UNUSED(rules);
}

// ─── Network capture control ─────────────────────────────────────────────────

void PiggyServer::startCapture(const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    if (!ctx.captureActive) {
        ctx.page->runJavaScript(NetworkCapture::captureScript());
        ctx.captureActive = true;
        qDebug() << "[PiggyServer] Capture started for tab" << tabId;
    }
}

void PiggyServer::stopCapture(const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    m_tabs[tabId].captureActive = false;
}

bool PiggyServer::isCapturing(const QString &tabId) const {
    return m_tabs.contains(tabId) && m_tabs[tabId].captureActive;
}

// ─── Capture signal handlers ─────────────────────────────────────────────────

void PiggyServer::onRequestCaptured(const CapturedRequest &req, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive)
        m_tabs[tabId].capturedRequests.append(req);
}

void PiggyServer::onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive)
        m_tabs[tabId].capturedWsFrames.append(frame);
}

void PiggyServer::onCookieCaptured(const CapturedCookie &cookie, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    m_tabs[tabId].capturedCookies.append(cookie);
}

void PiggyServer::onCookieRemoved(const QString &name, const QString &domain,
                                   const QString &tabId) {
    Q_UNUSED(name); Q_UNUSED(domain); Q_UNUSED(tabId);
}

void PiggyServer::onStorageCaptured(const QString &origin, const QString &key,
                                     const QString &value, const QString &storageType,
                                     const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    if (m_tabs[tabId].captureActive)
        m_tabs[tabId].storageEntries.append({storageType+":"+origin+":"+key, value});
}

void PiggyServer::onExposedFunctionCalled(const QString &name,
                                           const QString &callId,
                                           const QString &data,
                                           const QString &tabId) {
    // Broadcast to all connected clients as an event
    QJsonObject event;
    event["type"]   = "event";
    event["event"]  = "exposed_call";
    event["tabId"]  = tabId;
    event["name"]   = name;
    event["callId"] = callId;
    event["data"]   = data;

    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_clients) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// ─── Command router ──────────────────────────────────────────────────────────

void PiggyServer::handleCommand(const QJsonObject &cmd, QLocalSocket *client) {
    QString id          = cmd["id"].toString();
    QString c           = cmd["cmd"].toString();
    QJsonObject payload = cmd["payload"].toObject();
    QString tabId       = payload["tabId"].toString();

    // ── tab.new ───────────────────────────────────────────────────────────────
    if (c == "tab.new") {
        respond(client, id, true, createTab());
        return;
    }

    // ── tab.close ─────────────────────────────────────────────────────────────
    if (c == "tab.close") {
        if (tabId.isEmpty()) { respond(client, id, false, "tab.close requires tabId"); return; }
        closeTab(tabId);
        respond(client, id, true, "closed");
        return;
    }

    // ── tab.list ──────────────────────────────────────────────────────────────
    if (c == "tab.list") {
        QJsonArray arr;
        arr.append("default");
        for (const QString &k : m_tabs.keys()) arr.append(k);
        respond(client, id, true, arr.toVariantList());
        return;
    }

    // ── navigate ──────────────────────────────────────────────────────────────
    if (c == "navigate") {
        navigatePage(payload["url"].toString(), client, id, tabId);
        return;
    }

    // ── reload ────────────────────────────────────────────────────────────────
    if (c == "reload") {
        auto *p = page(tabId);
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok){ respond(client, id, ok, ok?"reloaded":"reload failed"); },
            Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Reload);
        return;
    }

    // ── go.back / go.forward ─────────────────────────────────────────────────
    if (c == "go.back") {
        auto *p = page(tabId);
        if (!p->history()->canGoBack()) {
            respond(client, id, false, "no history to go back"); return;
        }
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok){ respond(client, id, ok, ok?"back":"back failed"); },
            Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Back);
        return;
    }
    if (c == "go.forward") {
        auto *p = page(tabId);
        if (!p->history()->canGoForward()) {
            respond(client, id, false, "no history to go forward"); return;
        }
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok){ respond(client, id, ok, ok?"forward":"forward failed"); },
            Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Forward);
        return;
    }

    // ── page.url / page.title / page.content ─────────────────────────────────
    if (c == "page.url") {
        respond(client, id, true, page(tabId)->url().toString()); return;
    }
    if (c == "page.title") {
        respond(client, id, true, page(tabId)->title()); return;
    }
    if (c == "page.content") {
        page(tabId)->toHtml([=](const QString &html){
            respond(client, id, true, html);
        });
        return;
    }

    // ── screenshot / pdf ─────────────────────────────────────────────────────
    if (c == "screenshot") { doScreenshot(client, id, tabId); return; }
    if (c == "pdf")        { doPdf(client, id, tabId);        return; }

    // ── image blocking ───────────────────────────────────────────────────────
    if (c == "intercept.block.images")   { setImageBlocking(tabId, true);  respond(client,id,true,"images blocked");   return; }
    if (c == "intercept.unblock.images") { setImageBlocking(tabId, false); respond(client,id,true,"images unblocked"); return; }

    // ── wait commands ────────────────────────────────────────────────────────
    if (c == "wait.navigation") {
        auto *p = page(tabId);
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok){ respond(client, id, ok, ok?"navigated":"navigation failed"); },
            Qt::SingleShotConnection);
        return;
    }
    if (c == "wait.selector") {
        QString selector = payload["selector"].toString();
        int timeout      = payload["timeout"].toInt(10000);
        auto *p          = page(tabId);
        struct PollState { int elapsed = 0; };
        auto *state = new PollState();
        auto *timer = new QTimer(this);
        timer->setInterval(100);
        connect(timer, &QTimer::timeout, this, [=]() mutable {
            state->elapsed += 100;
            QString js = QString("(function(){ return !!document.querySelector('%1'); })()").arg(selector);
            p->runJavaScript(js, [=](const QVariant &found) {
                if (found.toBool()) {
                    timer->stop(); timer->deleteLater(); delete state;
                    respond(client, id, true, "found");
                } else if (state->elapsed >= timeout) {
                    timer->stop(); timer->deleteLater(); delete state;
                    respond(client, id, false, "timeout waiting for selector: " + selector);
                }
            });
        });
        timer->start();
        return;
    }
    if (c == "wait.response") {
        QTimer::singleShot(0, this, [=](){ respond(client, id, true, "response ready"); });
        return;
    }

    // ── search / fetch ───────────────────────────────────────────────────────
    if (c == "search.css") {
        page(tabId)->runJavaScript(PiggyTab::domExtractorJS(),
            [=](const QVariant &r){ respond(client, id, true, r); });
        return;
    }
    if (c == "search.id") {
        QString js = QString(
            "(function(){ var el=document.getElementById('%1');"
            "if(!el) return null;"
            "return {tag:el.tagName.toLowerCase(),id:el.id,cls:el.className,"
            "text:el.innerText.slice(0,200),html:el.innerHTML.slice(0,500)}; })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "fetch.text") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "return el?el.innerText.trim():null; })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "fetch.links") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('a')).map(a=>a.href).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "fetch.links.all") {
        page(tabId)->runJavaScript(
            "(function(){ return Array.from(document.querySelectorAll('a'))"
            ".map(a=>a.href).filter(Boolean); })()",
            [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "fetch.image") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('img')).map(i=>i.src).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }

    // ── interactions ─────────────────────────────────────────────────────────
    if (c == "click") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(el){el.click();return true;} return false; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "dblclick") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('dblclick',{bubbles:true,cancelable:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "hover") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('mouseover',{bubbles:true}));"
            "el.dispatchEvent(new MouseEvent('mouseenter',{bubbles:false}));"
            "return true; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "type") {
        QString text = payload["text"].toString();
        text.replace("\\","\\\\").replace("'","\\'");
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.focus(); el.value='%2';"
            "el.dispatchEvent(new Event('input',{bubbles:true}));"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString(), text);
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "select") {
        QString val = payload["value"].toString();
        val.replace("\\","\\\\").replace("'","\\'");
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.value='%2';"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString(), val);
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "scroll.to") {
        QString js = QString(
            "(function(){ var el=document.querySelector('%1');"
            "if(!el) return false;"
            "el.scrollIntoView({behavior:'smooth',block:'center'});"
            "return true; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "scroll.by") {
        int px = payload["px"].toInt(300);
        page(tabId)->runJavaScript(
            QString("window.scrollBy({top:%1,behavior:'smooth'}); true;").arg(px),
            [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "keyboard.press") {
        QString key = payload["key"].toString();
        key.replace("'","\\'");
        QString js = QString(
            "(function(){"
            "var el=document.activeElement||document.body;"
            "['keydown','keypress','keyup'].forEach(function(t){"
            "  el.dispatchEvent(new KeyboardEvent(t,{key:'%1',bubbles:true,cancelable:true}));"
            "});"
            "return true; })()"
        ).arg(key);
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "keyboard.combo") {
        QString combo = payload["combo"].toString();
        QStringList parts = combo.split('+');
        QString mainKey = parts.last(); mainKey.replace("'","\\'");
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
              ctrl  ? "true":"false",
              shift ? "true":"false",
              alt   ? "true":"false");
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "mouse.move") {
        int x = payload["x"].toInt(), y = payload["y"].toInt();
        page(tabId)->runJavaScript(
            QString("document.dispatchEvent(new MouseEvent('mousemove',{clientX:%1,clientY:%2,bubbles:true})); true;")
                .arg(x).arg(y),
            [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "mouse.drag") {
        QJsonObject from = payload["from"].toObject(), to = payload["to"].toObject();
        int fx=from["x"].toInt(), fy=from["y"].toInt();
        int tx=to["x"].toInt(),   ty=to["y"].toInt();
        QString js = QString(
            "(function(){"
            "var el=document.elementFromPoint(%1,%2);"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('mousedown',{clientX:%1,clientY:%2,bubbles:true}));"
            "document.dispatchEvent(new MouseEvent('mousemove',{clientX:%3,clientY:%4,bubbles:true}));"
            "document.dispatchEvent(new MouseEvent('mouseup',  {clientX:%3,clientY:%4,bubbles:true}));"
            "return true; })()"
        ).arg(fx).arg(fy).arg(tx).arg(ty);
        page(tabId)->runJavaScript(js, [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }
    if (c == "evaluate") {
        page(tabId)->runJavaScript(payload["js"].toString(),
            [=](const QVariant &r){ respond(client,id,true,r); });
        return;
    }

    // ── cookie commands ───────────────────────────────────────────────────────
    if (c == "cookie.set") {
        QString name   = payload["name"].toString();
        QString value  = payload["value"].toString();
        QString domain = payload["domain"].toString();
        QString path   = payload["path"].toString("/");
        bool httpOnly  = payload["httpOnly"].toBool(false);
        bool secure    = payload["secure"].toBool(false);
        qint64 expiry  = payload["expiry"].toVariant().toLongLong();
        if (name.isEmpty() || domain.isEmpty()) {
            respond(client, id, false, "cookie.set requires name and domain"); return;
        }
        QNetworkCookie cookie;
        cookie.setName(name.toUtf8()); cookie.setValue(value.toUtf8());
        cookie.setDomain(domain); cookie.setPath(path);
        cookie.setHttpOnly(httpOnly); cookie.setSecure(secure);
        if (expiry > 0) cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(expiry));
        page(tabId)->profile()->cookieStore()->setCookie(cookie, QUrl("https://"+domain));
        respond(client, id, true, "cookie set");
        return;
    }
    if (c == "cookie.get") {
        respond(client, id, false, "cookie.get not implemented (async)"); return;
    }
    if (c == "cookie.delete") {
        QString name=payload["name"].toString(), domain=payload["domain"].toString();
        if (name.isEmpty()||domain.isEmpty()) {
            respond(client,id,false,"cookie.delete requires name and domain"); return;
        }
        QNetworkCookie cookie; cookie.setName(name.toUtf8()); cookie.setDomain(domain);
        page(tabId)->profile()->cookieStore()->deleteCookie(cookie, QUrl("https://"+domain));
        respond(client, id, true, "cookie deleted");
        return;
    }
    if (c == "cookie.list") {
        respond(client, id, false, "cookie.list not implemented"); return;
    }

    // ── intercept rules ───────────────────────────────────────────────────────
    if (c == "intercept.rule.add") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        InterceptRule rule;
        rule.urlPattern = payload["pattern"].toString();
        rule.block      = payload["block"].toBool(false);
        rule.redirectUrl= payload["redirect"].toString();
        QJsonObject hdrs= payload["setHeaders"].toObject();
        for (auto it=hdrs.begin(); it!=hdrs.end(); ++it)
            rule.setHeaders[it.key()] = it.value().toString();
        QJsonArray rem  = payload["removeHeaders"].toArray();
        for (auto v : rem) rule.removeHeaders[v.toString()] = "";
        m_tabs[tabId].rules.append(rule);
        applyInterceptRules(m_tabs[tabId].interceptor, m_tabs[tabId].rules);
        respond(client, id, true, "rule added");
        return;
    }
    if (c == "intercept.rule.clear") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        m_tabs[tabId].rules.clear();
        applyInterceptRules(m_tabs[tabId].interceptor, {});
        respond(client, id, true, "rules cleared");
        return;
    }

    // ── capture commands ──────────────────────────────────────────────────────
    if (c == "capture.start") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        startCapture(tabId); respond(client,id,true,"capture started"); return;
    }
    if (c == "capture.stop") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        stopCapture(tabId); respond(client,id,true,"capture stopped"); return;
    }
    if (c == "capture.requests") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        QJsonArray arr;
        for (const auto &req : m_tabs[tabId].capturedRequests) {
            QJsonObject o;
            o["method"]=req.method; o["url"]=req.url; o["status"]=req.status;
            o["type"]=req.type; o["mime"]=req.mimeType;
            o["reqHeaders"]=req.requestHeaders; o["reqBody"]=req.requestBody;
            o["resHeaders"]=req.responseHeaders; o["resBody"]=req.responseBody;
            o["size"]=req.size; o["timestamp"]=req.timestamp;
            arr.append(o);
        }
        respond(client, id, true, arr); return;
    }
    if (c == "capture.ws") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        QJsonArray arr;
        for (const auto &f : m_tabs[tabId].capturedWsFrames) {
            QJsonObject o;
            o["connectionId"]=f.connectionId; o["url"]=f.url;
            o["direction"]=f.direction; o["data"]=f.data;
            o["binary"]=f.isBinary; o["timestamp"]=f.timestamp;
            arr.append(o);
        }
        respond(client, id, true, arr); return;
    }
    if (c == "capture.cookies") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        QJsonArray arr;
        for (const auto &ck : m_tabs[tabId].capturedCookies) {
            QJsonObject o;
            o["name"]=ck.name; o["value"]=ck.value; o["domain"]=ck.domain;
            o["path"]=ck.path; o["httpOnly"]=ck.httpOnly;
            o["secure"]=ck.secure; o["expires"]=ck.expires;
            arr.append(o);
        }
        respond(client, id, true, arr); return;
    }
    if (c == "capture.storage") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        QJsonArray arr;
        for (const auto &p : m_tabs[tabId].storageEntries) {
            QJsonObject o; o["key"]=p.first; o["value"]=p.second;
            arr.append(o);
        }
        respond(client, id, true, arr); return;
    }
    if (c == "capture.clear") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        m_tabs[tabId].capturedRequests.clear();
        m_tabs[tabId].capturedWsFrames.clear();
        m_tabs[tabId].capturedCookies.clear();
        m_tabs[tabId].storageEntries.clear();
        respond(client, id, true, "capture cleared"); return;
    }

    // ── session export / import ───────────────────────────────────────────────
    if (c == "session.export") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        QJsonObject root;
        root["url"] = page(tabId)->url().toString();
        QJsonArray reqArr;
        for (const auto &r : m_tabs[tabId].capturedRequests) {
            QJsonObject o;
            o["method"]=r.method; o["url"]=r.url; o["status"]=r.status;
            o["type"]=r.type; o["mime"]=r.mimeType;
            o["reqHeaders"]=r.requestHeaders; o["reqBody"]=r.requestBody;
            o["resHeaders"]=r.responseHeaders; o["resBody"]=r.responseBody;
            reqArr.append(o);
        }
        root["requests"] = reqArr;
        QJsonArray wsArr;
        for (const auto &f : m_tabs[tabId].capturedWsFrames) {
            QJsonObject o;
            o["url"]=f.url; o["direction"]=f.direction;
            o["data"]=f.data; o["binary"]=f.isBinary;
            wsArr.append(o);
        }
        root["ws"] = wsArr;
        QJsonArray ckArr;
        for (const auto &ck : m_tabs[tabId].capturedCookies) {
            QJsonObject o;
            o["name"]=ck.name; o["value"]=ck.value; o["domain"]=ck.domain;
            ckArr.append(o);
        }
        root["cookies"] = ckArr;
        respond(client, id, true,
                QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
        return;
    }
    if (c == "session.import") {
        if (!m_tabs.contains(tabId)) { respond(client,id,false,"invalid tabId"); return; }
        QJsonDocument doc = QJsonDocument::fromJson(payload["data"].toString().toUtf8());
        if (!doc.isObject()) { respond(client,id,false,"invalid JSON data"); return; }
        QJsonObject root = doc.object();
        TabContext &ctx  = m_tabs[tabId];
        for (auto v : root["requests"].toArray()) {
            CapturedRequest req; QJsonObject o=v.toObject();
            req.method=o["method"].toString(); req.url=o["url"].toString();
            req.status=o["status"].toString(); req.type=o["type"].toString();
            req.mimeType=o["mime"].toString();
            req.requestHeaders=o["reqHeaders"].toString();
            req.requestBody=o["reqBody"].toString();
            req.responseHeaders=o["resHeaders"].toString();
            req.responseBody=o["resBody"].toString();
            ctx.capturedRequests.append(req);
        }
        for (auto v : root["ws"].toArray()) {
            WebSocketFrame f; QJsonObject o=v.toObject();
            f.url=o["url"].toString(); f.direction=o["direction"].toString();
            f.data=o["data"].toString(); f.isBinary=o["binary"].toBool();
            ctx.capturedWsFrames.append(f);
        }
        for (auto v : root["cookies"].toArray()) {
            CapturedCookie ck; QJsonObject o=v.toObject();
            ck.name=o["name"].toString(); ck.value=o["value"].toString();
            ck.domain=o["domain"].toString();
            ctx.capturedCookies.append(ck);
        }
        respond(client, id, true, "session imported"); return;
    }

    // ── expose.function ───────────────────────────────────────────────────────
    if (c == "expose.function") {
        if (!m_tabs.contains(tabId)) { respond(client, id, false, "invalid tabId"); return; }
        QString fnName = payload["name"].toString();
        if (fnName.isEmpty()) { respond(client, id, false, "name required"); return; }

        TabContext &ctx = m_tabs[tabId];
        
        // Track which functions are exposed for this tab
        if (!ctx.exposedFunctions.contains(fnName)) {
            ctx.exposedFunctions.append(fnName);
        }

        // Inject the JS stub
        auto *p = page(tabId);
        p->runJavaScript(NetworkCapture::exposeFunctionScript(fnName));

        // Also inject via profile scripts so it survives navigation
        QWebEngineScript script;
        script.setName("nothing_expose_" + tabId + "_" + fnName);
        script.setSourceCode(NetworkCapture::exposeFunctionScript(fnName));
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setRunsOnSubFrames(true);
        p->profile()->scripts()->insert(script);

        // Wire up the capture signal for this tab if not already done
        if (!ctx.exposedConnected) {
            connect(ctx.capture, &NetworkCapture::exposedFunctionCalled, this,
                [this, tabId](const QString &name, const QString &callId, const QString &data) {
                    onExposedFunctionCalled(name, callId, data, tabId);
                });
            ctx.exposedConnected = true;
        }

        respond(client, id, true, "exposed: " + fnName);
        return;
    }

    // ── exposed.result ────────────────────────────────────────────────────────
    if (c == "exposed.result") {
        if (!m_tabs.contains(tabId)) { respond(client, id, false, "invalid tabId"); return; }
        QString callId  = payload["callId"].toString();
        QString result  = payload["result"].toString();
        bool    isError = payload["isError"].toBool(false);

        QString js = QString(
            "if (window.__NOTHING_RESOLVE_EXPOSED__) {"
            "  window.__NOTHING_RESOLVE_EXPOSED__('%1', '%2', %3);"
            "}"
        ).arg(callId, result.replace("\\", "\\\\").replace("'", "\\'"), isError ? "true" : "false");

        page(tabId)->runJavaScript(js);
        respond(client, id, true, "resolved");
        return;
    }

    respond(client, id, false, "unknown command: " + c);
}

// ─── Navigate ────────────────────────────────────────────────────────────────

void PiggyServer::navigatePage(const QString &url, QLocalSocket *client,
                                const QString &reqId, const QString &tabId) {
    auto *p = page(tabId);
    connect(p, &QWebEnginePage::loadFinished, this,
        [=](bool ok){ respond(client, reqId, ok, ok?"loaded":"load failed"); },
        Qt::SingleShotConnection);
    p->load(QUrl(url));
}

// ─── Respond ─────────────────────────────────────────────────────────────────

void PiggyServer::respond(QLocalSocket *client, const QString &id,
                           bool ok, const QVariant &data) {
    if (!client || client->state() != QLocalSocket::ConnectedState) return;
    QJsonObject res;
    res["id"] = id;
    res["ok"] = ok;
    if (data.typeId() == QMetaType::QString) {
        res["data"] = data.toString();
    } else if (data.canConvert<QJsonArray>()) {
        res["data"] = data.value<QJsonArray>();
    } else if (data.canConvert<QJsonObject>()) {
        res["data"] = data.value<QJsonObject>();
    } else {
        QJsonDocument d = QJsonDocument::fromVariant(data);
        if (d.isNull())        res["data"] = data.toString();
        else if (d.isArray())  res["data"] = d.array();
        else                   res["data"] = d.object();
    }
    client->write(QJsonDocument(res).toJson(QJsonDocument::Compact) + "\n");
    client->flush();
}