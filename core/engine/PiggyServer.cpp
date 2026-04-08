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
    QWebEnginePage *p = m_piggy
        ? new QWebEnginePage(m_piggy->getPage()->profile(), this)
        : new QWebEnginePage(m_ownProfile, this);
    m_tabs.insert(id, p);
    qDebug() << "[PiggyServer] Tab created:" << id;
    return id;
}

void PiggyServer::closeTab(const QString &tabId) {
    if (!m_tabs.contains(tabId)) return;
    m_tabs.take(tabId)->deleteLater();
    qDebug() << "[PiggyServer] Tab closed:" << tabId;
}

QWebEnginePage* PiggyServer::page(const QString &tabId) {
    if (!tabId.isEmpty() && tabId != "default") {
        auto it = m_tabs.find(tabId);
        if (it != m_tabs.end()) return it.value();
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
    for (auto *p : m_tabs) p->deleteLater();
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

// ─── Command router ──────────────────────────────────────────────────────────

void PiggyServer::handleCommand(const QJsonObject &cmd, QLocalSocket *client) {
    QString id          = cmd["id"].toString();
    QString c           = cmd["cmd"].toString();
    QJsonObject payload = cmd["payload"].toObject();
    QString tabId       = payload["tabId"].toString(); // optional in every cmd

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

    // ── search.css ────────────────────────────────────────────────────────────
    if (c == "search.css") {
        page(tabId)->runJavaScript(PiggyTab::domExtractorJS(),
            [=](const QVariant &result) { respond(client, id, true, result); });
        return;
    }

    // ── search.id ─────────────────────────────────────────────────────────────
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

    // ── fetch.text ────────────────────────────────────────────────────────────
    if (c == "fetch.text") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "return el ? el.innerText.trim() : null; })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }

    // ── fetch.links ───────────────────────────────────────────────────────────
    if (c == "fetch.links") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('a')).map(a => a.href).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }

    // ── fetch.image ───────────────────────────────────────────────────────────
    if (c == "fetch.image") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(!el) return [];"
            "return Array.from(el.querySelectorAll('img')).map(i => i.src).filter(Boolean); })()"
        ).arg(payload["query"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }

    // ── click ─────────────────────────────────────────────────────────────────
    if (c == "click") {
        QString js = QString(
            "(function(){ var el = document.querySelector('%1');"
            "if(el){ el.click(); return true; } return false; })()"
        ).arg(payload["selector"].toString());
        page(tabId)->runJavaScript(js, [=](const QVariant &r) { respond(client, id, true, r); });
        return;
    }

    // ── type ──────────────────────────────────────────────────────────────────
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

    // ── screenshot ────────────────────────────────────────────────────────────
    if (c == "screenshot") {
        if (m_piggy)
            respond(client, id, false, "screenshot: use headful grab");
        else
            respond(client, id, false, "screenshot: headless render pending");
        return;
    }

    // ── evaluate ──────────────────────────────────────────────────────────────
    if (c == "evaluate") {
        page(tabId)->runJavaScript(payload["js"].toString(), [=](const QVariant &r) {
            respond(client, id, true, r);
        });
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
    if (data.typeId() == QMetaType::QString)
        res["data"] = data.toString();
    else {
        QJsonDocument d = QJsonDocument::fromVariant(data);
        res["data"] = d.isNull() ? QJsonValue(data.toString()) : d.object();
    }
    client->write(QJsonDocument(res).toJson(QJsonDocument::Compact) + "\n");
    client->flush();
}