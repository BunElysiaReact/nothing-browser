#include "PiggyServer.h"
#include "../tabs/PiggyTab.h"
#include "NetworkCapture.h"
#include "Interceptor.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineHistory>
#include <QWebEngineCookieStore>
#include <QPageLayout>
#include <QPageSize>
#include <QMarginsF>
#include <QTimer>
#include <QRegularExpression>

PiggyServer::PiggyServer(PiggyTab *piggy, QObject *parent)
    : QObject(parent), m_piggy(piggy)
{
    if (!m_piggy) {
        m_ownProfile = new QWebEngineProfile(this);
        m_ownProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_ownProfile->setPersistentCookiesPolicy(
            QWebEngineProfile::NoPersistentCookies);
        m_ownPage = new QWebEnginePage(m_ownProfile, this);
    }
}

PiggyServer::~PiggyServer() { stop(); }

// ─── Tab management ──────────────────────────────────────────────────────────

QString PiggyServer::createTab() {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QWebEngineProfile *profile = m_piggy ? m_piggy->getPage()->profile() : m_ownProfile;
    QWebEnginePage *p = new QWebEnginePage(profile, this);

    // Create interceptor and attach to profile
    Interceptor *interceptor = new Interceptor(this);
    profile->setUrlRequestInterceptor(interceptor);

    // Create network capture and attach
    NetworkCapture *capture = new NetworkCapture(this);
    capture->attachToPage(p, profile);

    // Connect capture signals
    connect(capture, &NetworkCapture::requestCaptured, this,
            [this, id](const CapturedRequest &req) { onRequestCaptured(req, id); });
    connect(capture, &NetworkCapture::wsFrameCaptured, this,
            [this, id](const WebSocketFrame &frame) { onWsFrameCaptured(frame, id); });
    connect(capture, &NetworkCapture::cookieCaptured, this,
            [this, id](const CapturedCookie &cookie) { onCookieCaptured(cookie, id); });
    connect(capture, &NetworkCapture::cookieRemoved, this,
            [this, id](const QString &name, const QString &domain) { onCookieRemoved(name, domain, id); });
    connect(capture, &NetworkCapture::storageCaptured, this,
            [this, id](const QString &origin, const QString &key, const QString &value, const QString &type) {
                onStorageCaptured(origin, key, value, type, id);
            });

    TabContext ctx;
    ctx.page = p;
    ctx.interceptor = interceptor;
    ctx.capture = capture;
    ctx.imageBlocked = false;
    ctx.captureActive = false;
    m_tabs.insert(id, ctx);

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
    qDebug() << "[PiggyServer] Tab closed:" << tabId;
}

QWebEnginePage* PiggyServer::page(const QString &tabId) {
    if (!tabId.isEmpty() && tabId != "default") {
        auto it = m_tabs.find(tabId);
        if (it != m_tabs.end()) return it.value().page;
        qWarning() << "[PiggyServer] Unknown tabId:" << tabId << "— falling back to default";
    }
    return m_piggy ? m_piggy->getPage() : m_ownPage;
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
        connect(client, &QLocalSocket::readyRead, this, &PiggyServer::onClientData);
        connect(client, &QLocalSocket::disconnected, this, &PiggyServer::onClientDisconnected);
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

// ─── Screenshot / PDF helpers ─────────────────────────────────────────────────

void PiggyServer::doScreenshot(QLocalSocket *client, const QString &id, const QString &tabId) {
    auto *p = page(tabId);
    const QString js = QStringLiteral(
        "(function(){"
        "  try {"
        "    var w = Math.min(document.documentElement.scrollWidth  || 1280, 8192);"
        "    var h = Math.min(document.documentElement.scrollHeight || 800, 16384);"
        "    var c = document.createElement('canvas');"
        "    c.width = w; c.height = h;"
        "    var ctx = c.getContext('2d');"
        "    ctx.fillStyle = '#ffffff';"
        "    ctx.fillRect(0, 0, w, h);"
        "    ctx.fillStyle = '#000000';"
        "    ctx.font = '16px sans-serif';"
        "    ctx.fillText('screenshot: ' + document.title, 20, 40);"
        "    return c.toDataURL('image/png').split(',')[1];"
        "  } catch(e) { return null; }"
        "})()"
    );
    p->runJavaScript(js, [=](const QVariant &r) {
        if (r.isNull() || !r.isValid())
            respond(client, id, false, "screenshot: JS render failed");
        else
            respond(client, id, true, r.toString());
    });
}

void PiggyServer::doPdf(QLocalSocket *client, const QString &id, const QString &tabId) {
    auto *p = page(tabId);
    QPageLayout layout(QPageSize(QPageSize::A4),
                       QPageLayout::Portrait,
                       QMarginsF(15, 15, 15, 15),
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
    QWebEngineProfile *profile = page(tabId)->profile();
    // Qt doesn't provide a synchronous way to list all cookies; we'd need async.
    // For simplicity, we return empty list; actual cookie management uses async store.
    return {};
}

// ─── Interception rules ──────────────────────────────────────────────────────

void PiggyServer::applyInterceptRules(Interceptor *interceptor, const QVector<InterceptRule> &rules) {
    // In a real implementation, Interceptor would hold rules and evaluate them.
    // For simplicity, we'll store rules in TabContext and let Interceptor access them.
    // This requires Interceptor to have a pointer to its owning TabContext.
    Q_UNUSED(interceptor);
    // Placeholder – actual implementation would set rules on the interceptor.
}

// ─── Network capture control ─────────────────────────────────────────────────

void PiggyServer::startCapture(const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    if (!ctx.captureActive) {
        // Inject capture script
        ctx.page->runJavaScript(NetworkCapture::captureScript());
        ctx.captureActive = true;
        qDebug() << "[PiggyServer] Capture started for tab" << tabId;
    }
}

void PiggyServer::stopCapture(const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    ctx.captureActive = false;
    // No way to uninject script, but we can stop processing messages.
}

bool PiggyServer::isCapturing(const QString &tabId) const {
    return m_tabs.contains(tabId) && m_tabs[tabId].captureActive;
}

// ─── Capture signal handlers ─────────────────────────────────────────────────

void PiggyServer::onRequestCaptured(const CapturedRequest &req, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    if (ctx.captureActive)
        ctx.capturedRequests.append(req);
    // Optionally forward to connected clients via a dedicated notification.
}

void PiggyServer::onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    if (ctx.captureActive)
        ctx.capturedWsFrames.append(frame);
}

void PiggyServer::onCookieCaptured(const CapturedCookie &cookie, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    ctx.capturedCookies.append(cookie);
}

void PiggyServer::onCookieRemoved(const QString &name, const QString &domain, const QString &tabId) {
    Q_UNUSED(name); Q_UNUSED(domain); Q_UNUSED(tabId);
    // Could mark cookie as removed.
}

void PiggyServer::onStorageCaptured(const QString &origin, const QString &key,
                                    const QString &value, const QString &storageType, const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    TabContext &ctx = m_tabs[tabId];
    if (ctx.captureActive)
        ctx.storageEntries.append({storageType + ":" + origin + ":" + key, value});
}

// ─── Command router ──────────────────────────────────────────────────────────

void PiggyServer::handleCommand(const QJsonObject &cmd, QLocalSocket *client) {
    QString id          = cmd["id"].toString();
    QString c           = cmd["cmd"].toString();
    QJsonObject payload = cmd["payload"].toObject();
    QString tabId       = payload["tabId"].toString();

    // ── tab.new ───────────────────────────────────────────────────────────────
    if (c == "tab.new") {
        if (m_piggy) {
            respond(client, id, false, "tab.new not supported in headful mode");
            return;
        }
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
            [=](bool ok) { respond(client, id, ok, ok ? "reloaded" : "reload failed"); },
            Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Reload);
        return;
    }

    // ── go.back / go.forward ─────────────────────────────────────────────────
    if (c == "go.back") {
        auto *p = page(tabId);
        if (!p->history()->canGoBack()) {
            respond(client, id, false, "no history to go back");
            return;
        }
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok) { respond(client, id, ok, ok ? "back" : "back failed"); },
            Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Back);
        return;
    }
    if (c == "go.forward") {
        auto *p = page(tabId);
        if (!p->history()->canGoForward()) {
            respond(client, id, false, "no history to go forward");
            return;
        }
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok) { respond(client, id, ok, ok ? "forward" : "forward failed"); },
            Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Forward);
        return;
    }

    // ── page.url / page.title / page.content ─────────────────────────────────
    if (c == "page.url") {
        respond(client, id, true, page(tabId)->url().toString());
        return;
    }
    if (c == "page.title") {
        respond(client, id, true, page(tabId)->title());
        return;
    }
    if (c == "page.content") {
        page(tabId)->toHtml([=](const QString &html) {
            respond(client, id, true, html);
        });
        return;
    }

    // ── screenshot / pdf ─────────────────────────────────────────────────────
    if (c == "screenshot") {
        doScreenshot(client, id, tabId);
        return;
    }
    if (c == "pdf") {
        doPdf(client, id, tabId);
        return;
    }

    // ── image blocking ───────────────────────────────────────────────────────
    if (c == "intercept.block.images") {
        setImageBlocking(tabId, true);
        respond(client, id, true, "images blocked");
        return;
    }
    if (c == "intercept.unblock.images") {
        setImageBlocking(tabId, false);
        respond(client, id, true, "images unblocked");
        return;
    }

    // ── wait commands ────────────────────────────────────────────────────────
    if (c == "wait.navigation") {
        auto *p = page(tabId);
        connect(p, &QWebEnginePage::loadFinished, this,
            [=](bool ok) { respond(client, id, ok, ok ? "navigated" : "navigation failed"); },
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
            QString js = QString(
                "(function(){ return !!document.querySelector('%1'); })()"
            ).arg(selector);
            p->runJavaScript(js, [=](const QVariant &found) {
                if (found.toBool()) {
                    timer->stop();
                    timer->deleteLater();
                    delete state;
                    respond(client, id, true, "found");
                } else if (state->elapsed >= timeout) {
                    timer->stop();
                    timer->deleteLater();
                    delete state;
                    respond(client, id, false, "timeout waiting for selector: " + selector);
                }
            });
        });
        timer->start();
        return;
    }
    if (c == "wait.response") {
        QTimer::singleShot(0, this, [=]() {
            respond(client, id, true, "response ready");
        });
        return;
    }

    // ── search / fetch commands ──────────────────────────────────────────────
    if (c == "search.css") {
        page(tabId)->runJavaScript(PiggyTab::domExtractorJS(),
            [=](const QVariant &result) { respond(client, id, true, result); });
        return;
    }
    if (c == "search.id") {
        QString js = QString(
            "(function(){ var el = document.getElementById('%1');"
            "if(!el) return null;"
            "return { tag: el.tagName.toLowerCase(), id: el.id, cls: el.className,"
            "  text: el.innerText.slice(0,200), html: el.innerHTML.slice(0,500) }; })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "fetch.text") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "return el ? el.innerText.trim() : null; })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "fetch.links") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('a')).map(a => a.href).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "fetch.links.all") {
        QString js =
            "(function(){ return Array.from(document.querySelectorAll('a'))"
            ".map(a => a.href).filter(Boolean); })()";
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "fetch.image") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('img')).map(i => i.src).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }

    // ── interaction commands ─────────────────────────────────────────────────
    if (c == "click") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(el){ el.click(); return true; } return false; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "dblclick") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('dblclick',{bubbles:true,cancelable:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "hover") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('mouseover',{bubbles:true}));"
            "el.dispatchEvent(new MouseEvent('mouseenter',{bubbles:false}));"
            "return true; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "type") {
        QString text = payload["text"].toString();
        text.replace("\\", "\\\\").replace("'", "\\'");
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return false;"
            "el.focus(); el.value = '%2';"
            "el.dispatchEvent(new Event('input',{bubbles:true}));"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString(), text);
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "select") {
        QString val = payload["value"].toString();
        val.replace("\\", "\\\\").replace("'", "\\'");
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return false;"
            "el.value = '%2';"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(payload["selector"].toString(), val);
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "scroll.to") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return false;"
            "el.scrollIntoView({behavior:'smooth',block:'center'});"
            "return true; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "scroll.by") {
        int px = payload["px"].toInt(300);
        QString js = QString("window.scrollBy({top:%1,behavior:'smooth'}); true;").arg(px);
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "keyboard.press") {
        QString key = payload["key"].toString();
        key.replace("'", "\\'");
        QString js = QString(
            "(function(){"
            "var el = document.activeElement || document.body;"
            "['keydown','keypress','keyup'].forEach(function(t){"
            "  el.dispatchEvent(new KeyboardEvent(t,{key:'%1',bubbles:true,cancelable:true}));"
            "});"
            "return true; })()"
        ).arg(key);
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
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
            "var el = document.activeElement || document.body;"
            "var opts={key:'%1',ctrlKey:%2,shiftKey:%3,altKey:%4,bubbles:true,cancelable:true};"
            "['keydown','keypress','keyup'].forEach(function(t){"
            "  el.dispatchEvent(new KeyboardEvent(t,opts));"
            "});"
            "return true; })()"
        ).arg(mainKey,
              ctrl  ? "true" : "false",
              shift ? "true" : "false",
              alt   ? "true" : "false");
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "mouse.move") {
        int x = payload["x"].toInt();
        int y = payload["y"].toInt();
        QString js = QString(
            "document.dispatchEvent(new MouseEvent('mousemove',{clientX:%1,clientY:%2,bubbles:true}));"
            "true;"
        ).arg(x).arg(y);
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "mouse.drag") {
        QJsonObject from = payload["from"].toObject();
        QJsonObject to   = payload["to"].toObject();
        int fx = from["x"].toInt(), fy = from["y"].toInt();
        int tx = to["x"].toInt(),   ty = to["y"].toInt();
        QString js = QString(
            "(function(){"
            "var el = document.elementFromPoint(%1,%2);"
            "if(!el) return false;"
            "el.dispatchEvent(new MouseEvent('mousedown',{clientX:%1,clientY:%2,bubbles:true}));"
            "document.dispatchEvent(new MouseEvent('mousemove',{clientX:%3,clientY:%4,bubbles:true}));"
            "document.dispatchEvent(new MouseEvent('mouseup',  {clientX:%3,clientY:%4,bubbles:true}));"
            "return true; })()"
        ).arg(fx).arg(fy).arg(tx).arg(ty);
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }
    if (c == "evaluate") {
        page(tabId)->runJavaScript(payload["js"].toString(), [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // NEW: Cookie commands
    // ═══════════════════════════════════════════════════════════════════════════
    if (c == "cookie.set") {
        QString name   = payload["name"].toString();
        QString value  = payload["value"].toString();
        QString domain = payload["domain"].toString();
        QString path   = payload["path"].toString("/");
        bool httpOnly  = payload["httpOnly"].toBool(false);
        bool secure    = payload["secure"].toBool(false);
        qint64 expiry  = payload["expiry"].toVariant().toLongLong();

        if (name.isEmpty() || domain.isEmpty()) {
            respond(client, id, false, "cookie.set requires name and domain");
            return;
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

        auto *profile = page(tabId)->profile();
        profile->cookieStore()->setCookie(cookie, QUrl("https://" + domain));
        respond(client, id, true, "cookie set");
        return;
    }

    if (c == "cookie.get") {
        QString name   = payload["name"].toString();
        QString domain = payload["domain"].toString();
        // Qt cookie store is async; we'll need to query and respond asynchronously.
        // For simplicity, we respond with a placeholder.
        respond(client, id, false, "cookie.get not implemented (async)");
        return;
    }

    if (c == "cookie.delete") {
        QString name   = payload["name"].toString();
        QString domain = payload["domain"].toString();
        if (name.isEmpty() || domain.isEmpty()) {
            respond(client, id, false, "cookie.delete requires name and domain");
            return;
        }
        auto *profile = page(tabId)->profile();
        // We need to fetch cookie first, then delete; Qt's API is deleteCookie(name, origin)
        // For simplicity, we can construct a dummy cookie with same name/domain and call deleteCookie.
        QNetworkCookie cookie;
        cookie.setName(name.toUtf8());
        cookie.setDomain(domain);
        profile->cookieStore()->deleteCookie(cookie, QUrl("https://" + domain));
        respond(client, id, true, "cookie deleted");
        return;
    }

    if (c == "cookie.list") {
        // Async – not implemented fully; would require callback.
        respond(client, id, false, "cookie.list not implemented");
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Interception rules (request‑side)
    // ═══════════════════════════════════════════════════════════════════════════
    if (c == "intercept.rule.add") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        InterceptRule rule;
        rule.urlPattern = payload["pattern"].toString();
        rule.block = payload["block"].toBool(false);
        rule.redirectUrl = payload["redirect"].toString();
        QJsonObject headersObj = payload["setHeaders"].toObject();
        for (auto it = headersObj.begin(); it != headersObj.end(); ++it)
            rule.setHeaders[it.key()] = it.value().toString();
        QJsonArray removeArr = payload["removeHeaders"].toArray();
        for (auto v : removeArr)
            rule.removeHeaders[v.toString()] = "";

        m_tabs[tabId].rules.append(rule);
        applyInterceptRules(m_tabs[tabId].interceptor, m_tabs[tabId].rules);
        respond(client, id, true, "rule added");
        return;
    }

    if (c == "intercept.rule.clear") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        m_tabs[tabId].rules.clear();
        applyInterceptRules(m_tabs[tabId].interceptor, {});
        respond(client, id, true, "rules cleared");
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Network capture commands
    // ═══════════════════════════════════════════════════════════════════════════
    if (c == "capture.start") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        startCapture(tabId);
        respond(client, id, true, "capture started");
        return;
    }

    if (c == "capture.stop") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        stopCapture(tabId);
        respond(client, id, true, "capture stopped");
        return;
    }

    if (c == "capture.requests") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        QJsonArray arr;
        for (const auto &req : m_tabs[tabId].capturedRequests) {
            QJsonObject obj;
            obj["method"] = req.method;
            obj["url"] = req.url;
            obj["status"] = req.status;
            obj["type"] = req.type;
            obj["mime"] = req.mimeType;
            obj["reqHeaders"] = req.requestHeaders;
            obj["reqBody"] = req.requestBody;
            obj["resHeaders"] = req.responseHeaders;
            obj["resBody"] = req.responseBody;
            obj["size"] = req.size;
            obj["timestamp"] = req.timestamp;
            arr.append(obj);
        }
        respond(client, id, true, arr);
        return;
    }

    if (c == "capture.ws") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        QJsonArray arr;
        for (const auto &f : m_tabs[tabId].capturedWsFrames) {
            QJsonObject obj;
            obj["connectionId"] = f.connectionId;
            obj["url"] = f.url;
            obj["direction"] = f.direction;
            obj["data"] = f.data;
            obj["binary"] = f.isBinary;
            obj["timestamp"] = f.timestamp;
            arr.append(obj);
        }
        respond(client, id, true, arr);
        return;
    }

    if (c == "capture.cookies") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        QJsonArray arr;
        for (const auto &c : m_tabs[tabId].capturedCookies) {
            QJsonObject obj;
            obj["name"] = c.name;
            obj["value"] = c.value;
            obj["domain"] = c.domain;
            obj["path"] = c.path;
            obj["httpOnly"] = c.httpOnly;
            obj["secure"] = c.secure;
            obj["expires"] = c.expires;
            arr.append(obj);
        }
        respond(client, id, true, arr);
        return;
    }

    if (c == "capture.storage") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        QJsonArray arr;
        for (const auto &p : m_tabs[tabId].storageEntries) {
            QJsonObject obj;
            obj["key"] = p.first;
            obj["value"] = p.second;
            arr.append(obj);
        }
        respond(client, id, true, arr);
        return;
    }

    if (c == "capture.clear") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        m_tabs[tabId].capturedRequests.clear();
        m_tabs[tabId].capturedWsFrames.clear();
        m_tabs[tabId].capturedCookies.clear();
        m_tabs[tabId].storageEntries.clear();
        respond(client, id, true, "capture cleared");
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Session export/import
    // ═══════════════════════════════════════════════════════════════════════════
    if (c == "session.export") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        QJsonObject root;
        root["url"] = page(tabId)->url().toString();
        QJsonArray reqArr;
        for (const auto &r : m_tabs[tabId].capturedRequests) {
            QJsonObject o;
            o["method"] = r.method;
            o["url"] = r.url;
            o["status"] = r.status;
            o["type"] = r.type;
            o["mime"] = r.mimeType;
            o["reqHeaders"] = r.requestHeaders;
            o["reqBody"] = r.requestBody;
            o["resHeaders"] = r.responseHeaders;
            o["resBody"] = r.responseBody;
            reqArr.append(o);
        }
        root["requests"] = reqArr;
        QJsonArray wsArr;
        for (const auto &f : m_tabs[tabId].capturedWsFrames) {
            QJsonObject o;
            o["url"] = f.url;
            o["direction"] = f.direction;
            o["data"] = f.data;
            o["binary"] = f.isBinary;
            wsArr.append(o);
        }
        root["ws"] = wsArr;
        QJsonArray cookieArr;
        for (const auto &c : m_tabs[tabId].capturedCookies) {
            QJsonObject o;
            o["name"] = c.name;
            o["value"] = c.value;
            o["domain"] = c.domain;
            cookieArr.append(o);
        }
        root["cookies"] = cookieArr;

        QJsonDocument doc(root);
        respond(client, id, true, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        return;
    }

    if (c == "session.import") {
        if (!m_tabs.contains(tabId)) {
            respond(client, id, false, "invalid tabId");
            return;
        }
        QByteArray data = payload["data"].toString().toUtf8();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            respond(client, id, false, "invalid JSON data");
            return;
        }
        QJsonObject root = doc.object();
        // Optional: set URL?
        // For now, just restore captures into the tab context.
        TabContext &ctx = m_tabs[tabId];
        for (auto v : root["requests"].toArray()) {
            CapturedRequest req;
            QJsonObject o = v.toObject();
            req.method = o["method"].toString();
            req.url = o["url"].toString();
            req.status = o["status"].toString();
            req.type = o["type"].toString();
            req.mimeType = o["mime"].toString();
            req.requestHeaders = o["reqHeaders"].toString();
            req.requestBody = o["reqBody"].toString();
            req.responseHeaders = o["resHeaders"].toString();
            req.responseBody = o["resBody"].toString();
            ctx.capturedRequests.append(req);
        }
        for (auto v : root["ws"].toArray()) {
            WebSocketFrame f;
            QJsonObject o = v.toObject();
            f.url = o["url"].toString();
            f.direction = o["direction"].toString();
            f.data = o["data"].toString();
            f.isBinary = o["binary"].toBool();
            ctx.capturedWsFrames.append(f);
        }
        for (auto v : root["cookies"].toArray()) {
            CapturedCookie c;
            QJsonObject o = v.toObject();
            c.name = o["name"].toString();
            c.value = o["value"].toString();
            c.domain = o["domain"].toString();
            ctx.capturedCookies.append(c);
        }
        respond(client, id, true, "session imported");
        return;
    }

    respond(client, id, false, "unknown command: " + c);
}

// ─── Navigate ────────────────────────────────────────────────────────────────

void PiggyServer::navigatePage(const QString &url, QLocalSocket *client,
                               const QString &reqId, const QString &tabId) {
    auto *p = page(tabId);
    connect(p, &QWebEnginePage::loadFinished, this,
        [=](bool ok) { respond(client, reqId, ok, ok ? "loaded" : "load failed"); },
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
        if (d.isNull()) {
            res["data"] = data.toString();
        } else if (d.isArray()) {
            res["data"] = d.array();
        } else {
            res["data"] = d.object();
        }
    }
    client->write(QJsonDocument(res).toJson(QJsonDocument::Compact) + "\n");
    client->flush();
}