#include "PiggyInnerStorage.h"
#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineProfile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

static PiggyInnerStorage *s_innerStorage = nullptr;
PiggyInnerStorage *piggy_innerStorage() { return s_innerStorage; }
void piggy_innerStorageInit(PiggyServer *srv, const QString &path) {
    if (!s_innerStorage) s_innerStorage = new PiggyInnerStorage(srv, path, srv);
}

// ─── The JS hook that intercepts localStorage writes ──────────────────────────
static const QString kStorageHook = R"JS(
(function() {
    if (window.__piggyStorageHooked) return;
    window.__piggyStorageHooked = true;

    var _setItem = Storage.prototype.setItem;
    var _removeItem = Storage.prototype.removeItem;
    var _clear = Storage.prototype.clear;

    function snapshot() {
        var data = {};
        for (var i = 0; i < localStorage.length; i++) {
            var k = localStorage.key(i);
            data[k] = localStorage.getItem(k);
        }
        if (window.__piggyStorageCallback) {
            var oldTitle = document.title;
            document.title = '__PIGGY_STORAGE__' + JSON.stringify(data);
            document.title = oldTitle;
        }
    }

    Storage.prototype.setItem = function(key, value) {
        _setItem.apply(this, arguments);
        if (this === localStorage) snapshot();
    };
    Storage.prototype.removeItem = function(key) {
        _removeItem.apply(this, arguments);
        if (this === localStorage) snapshot();
    };
    Storage.prototype.clear = function() {
        _clear.apply(this, arguments);
        if (this === localStorage) snapshot();
    };
})();
)JS";

PiggyInnerStorage::PiggyInnerStorage(PiggyServer *srv,
                                      const QString &path,
                                      QObject *parent)
    : QObject(parent), m_srv(srv), m_path(path) {}

void PiggyInnerStorage::watchTab(const QString &tabId, QWebEnginePage *page) {
    m_pages[tabId] = page;

    QObject::connect(page, &QWebEnginePage::loadFinished, this,
        [this, tabId](bool ok) {
            onLoadFinished(ok, tabId);
        });
}

void PiggyInnerStorage::unwatchTab(const QString &tabId) {
    m_pages.remove(tabId);
}

void PiggyInnerStorage::onLoadFinished(bool ok, const QString &tabId) {
    if (!ok) return;
    injectIntoTab(tabId);
    installStorageHook(tabId);
}

void PiggyInnerStorage::injectIntoTab(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) return;

    QJsonObject saved = loadStorage();
    if (saved.isEmpty()) return;

    QString js = QString("(function(){ var d = %1; "
                         "Object.keys(d).forEach(function(k){ "
                         "localStorage.setItem(k, d[k]); }); })()")
        .arg(QString::fromUtf8(QJsonDocument(saved).toJson(QJsonDocument::Compact)));

    page->runJavaScript(js, [this, tabId, saved](const QVariant &) {
        QJsonObject ev;
        ev["type"]  = "event";
        ev["event"] = "storage:loaded";
        ev["tabId"] = tabId;
        ev["keys"]  = saved.size();
        broadcastEvent(ev);
        qDebug() << "[PiggyStorage] Injected" << saved.size()
                 << "localStorage keys into tab" << tabId;
    });
}

void PiggyInnerStorage::installStorageHook(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) return;

    page->runJavaScript(kStorageHook);

    QString callbackJs = R"JS(
        window.__piggyStorageCallback = function(json) {
            document.title = '__PIGGY_STORAGE__' + json;
        };
    )JS";
    page->runJavaScript(callbackJs);

    QObject::connect(page, &QWebEnginePage::titleChanged, this,
        [this, tabId](const QString &title) {
            if (!title.startsWith("__PIGGY_STORAGE__")) return;
            QString jsonStr = title.mid(17);
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (doc.isObject()) {
                saveStorage(tabId, doc.object());
            }
        });
}

void PiggyInnerStorage::saveStorage(const QString &tabId,
                                     const QJsonObject &data) {
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
    f.close();

    QJsonObject ev;
    ev["type"]  = "event";
    ev["event"] = "storage:saved";
    ev["tabId"] = tabId;
    ev["keys"]  = data.size();
    broadcastEvent(ev);

    qDebug() << "[PiggyStorage] Saved" << data.size()
             << "localStorage keys to" << m_path;
}

QJsonObject PiggyInnerStorage::loadStorage() const {
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

void PiggyInnerStorage::broadcastEvent(const QJsonObject &event) {
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_srv->clients()) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleStorage(PiggyServer *srv, const QString &c,
                          const QJsonObject &payload,
                          QLocalSocket *client, const QString &id,
                          const QString &tabId)
{
    if (c == "storage.dump") {
        auto *is = piggy_innerStorage();
        if (!is) { srv->respond(client, id, false, "innerstorage not initialized"); return true; }
        // loadStorage() is private — call dumpStorage() which is the public wrapper
        srv->respond(client, id, true, is->dumpStorage());
        return true;
    }

    if (c == "storage.clear") {
        auto *is = piggy_innerStorage();
        if (!is) { srv->respond(client, id, false, "innerstorage not initialized"); return true; }
        is->clearStorage();
        srv->respond(client, id, true, "storage cleared");
        return true;
    }

    return false;
}