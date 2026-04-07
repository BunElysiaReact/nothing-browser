#include "PiggyServer.h"
#include "../tabs/PiggyTab.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QLocalSocket>
#include <QWebEnginePage>
#include <QWebEngineProfile>

PiggyServer::PiggyServer(PiggyTab *piggy, QObject *parent)
    : QObject(parent), m_piggy(piggy)
{
    // headless — create own offscreen page with fresh profile
    if (!m_piggy) {
        m_ownProfile = new QWebEngineProfile(this);
        m_ownProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_ownProfile->setPersistentCookiesPolicy(
            QWebEngineProfile::NoPersistentCookies);
        m_ownPage = new QWebEnginePage(m_ownProfile, this);
    }
}

PiggyServer::~PiggyServer() { stop(); }

void PiggyServer::start() {
    // Remove stale socket from last run
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

// ─────────────────────────────────────────────────────────────────────────────
void PiggyServer::handleCommand(const QJsonObject &cmd, QLocalSocket *client) {
    QString id  = cmd["id"].toString();
    QString c   = cmd["cmd"].toString();
    QJsonObject payload = cmd["payload"].toObject();

    // ── navigate ──────────────────────────────────────────────────────────────
    if (c == "navigate") {
        navigatePage(payload["url"].toString(), client, id);
        return;
    }

    // ── search.css ────────────────────────────────────────────────────────────
    if (c == "search.css") {
        QString query = payload["query"].toString().toLower();
        page()->runJavaScript(PiggyTab::domExtractorJS(),
            [=](const QVariant &result) {
                // Filter nodes where cls contains query
                QJsonDocument d = QJsonDocument::fromVariant(result);
                // Return full tree — client filters; keeps server simple
                respond(client, id, true, result);
            });
        return;
    }

    // ── search.id ─────────────────────────────────────────────────────────────
    if (c == "search.id") {
        QString query = payload["query"].toString();
        QString js = QString(
            "(function(){ var el = document.getElementById('%1');"
            "if(!el) return null;"
            "return { tag: el.tagName.toLowerCase(),"
            "  id: el.id, cls: el.className,"
            "  text: el.innerText.slice(0,200),"
            "  html: el.innerHTML.slice(0,500) }; })()"
        ).arg(query);
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── fetch.text ────────────────────────────────────────────────────────────
    if (c == "fetch.text") {
        QString query = payload["query"].toString();
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "return el ? el.innerText.trim() : null; })()"
        ).arg(query);
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── fetch.links ───────────────────────────────────────────────────────────
    if (c == "fetch.links") {
        QString query = payload["query"].toString();
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('a'))"
            ".map(a => a.href).filter(Boolean); })()"
        ).arg(query);
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── fetch.image ───────────────────────────────────────────────────────────
    if (c == "fetch.image") {
        QString query = payload["query"].toString();
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('img'))"
            ".map(i => i.src).filter(Boolean); })()"
        ).arg(query);
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── click ─────────────────────────────────────────────────────────────────
    if (c == "click") {
        QString sel = payload["selector"].toString();
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(el){ el.click(); return true; } return false; })()"
        ).arg(sel);
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── type ──────────────────────────────────────────────────────────────────
    if (c == "type") {
        QString sel  = payload["selector"].toString();
        QString text = payload["text"].toString();
        // escape for JS string
        text.replace("\\", "\\\\").replace("'", "\\'");
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return false;"
            "el.focus(); el.value = '%2';"
            "el.dispatchEvent(new Event('input',{bubbles:true}));"
            "el.dispatchEvent(new Event('change',{bubbles:true}));"
            "return true; })()"
        ).arg(sel, text);
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── screenshot ────────────────────────────────────────────────────────────
    if (c == "screenshot") {
        // QWebEngineView needed for grabFramebuffer in headful
        // headless: use page()->toHtml as fallback until we add offscreen render
        QString path = payload["path"].toString();
        if (path.isEmpty()) path = "/tmp/piggy_screenshot.png";

        if (m_piggy) {
            // headful — grab from the visible view
            // accessed via m_piggy->mirror() — add a getter in PiggyTab
            respond(client, id, false, "screenshot: use headful grab — add PiggyTab::grabScreenshot()");
        } else {
            respond(client, id, false, "screenshot: headless render pending");
        }
        return;
    }

    // ── evaluate ──────────────────────────────────────────────────────────────
    if (c == "evaluate") {
        QString js = payload["js"].toString();
        page()->runJavaScript(js, [=](const QVariant &r) {
            respond(client, id, true, r);
        });
        return;
    }

    // ── unknown ───────────────────────────────────────────────────────────────
    respond(client, id, false, "unknown command: " + c);
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyServer::navigatePage(const QString &url, QLocalSocket *client,
                               const QString &reqId) {
    auto *p = page();
    // one-shot connection — respond after load
    connect(p, &QWebEnginePage::loadFinished, this,
        [=](bool ok) {
            respond(client, reqId, ok, ok ? "loaded" : "load failed");
        },
        Qt::SingleShotConnection);
    p->load(QUrl(url));
}

// ─────────────────────────────────────────────────────────────────────────────
void PiggyServer::respond(QLocalSocket *client, const QString &id,
                          bool ok, const QVariant &data) {
    if (!client || client->state() != QLocalSocket::ConnectedState) return;

    QJsonObject res;
    res["id"] = id;
    res["ok"] = ok;

    if (data.typeId() == QMetaType::QString)
        res["data"] = data.toString();
    else {
        QJsonDocument d = QJsonDocument::fromVariant(data);
        res["data"] = d.isNull() ? QJsonValue(data.toString()) : d.object();
    }

    client->write(QJsonDocument(res).toJson(QJsonDocument::Compact) + "\n");
    client->flush();
}

// ─────────────────────────────────────────────────────────────────────────────
QWebEnginePage* PiggyServer::page() {
    return m_piggy ? m_piggy->getPage() : m_ownPage;
}