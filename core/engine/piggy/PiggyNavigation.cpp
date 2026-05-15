#include "PiggyServer.h"
#include "../../tabs/PiggyTab.h"
#include <QWebEnginePage>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QTimer>
#include <QJsonDocument>

// ─── Forward decl ─────────────────────────────────────────────────────────────
QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Navigate ────────────────────────────────────────────────────────────────

void piggy_navigate(PiggyServer *srv, const QString &url,
                    QLocalSocket *client, const QString &reqId,
                    const QString &tabId) {
    auto *p = piggy_page(srv, tabId);
    QObject::connect(p, &QWebEnginePage::loadFinished, srv,
        [srv, client, reqId](bool ok) {
            srv->respond(client, reqId, ok, ok ? "loaded" : "load failed");
        }, Qt::SingleShotConnection);
    p->load(QUrl(url));
}

// ─── All navigation commands ──────────────────────────────────────────────────

bool piggy_handleNavigation(PiggyServer *srv, const QString &c,
                             const QJsonObject &payload,
                             QLocalSocket *client, const QString &id,
                             const QString &tabId) {
    auto *p = piggy_page(srv, tabId);

    if (c == "navigate") {
        piggy_navigate(srv, payload["url"].toString(), client, id, tabId);
        return true;
    }

    if (c == "reload") {
        QObject::connect(p, &QWebEnginePage::loadFinished, srv,
            [srv, client, id](bool ok) {
                srv->respond(client, id, ok, ok ? "reloaded" : "reload failed");
            }, Qt::SingleShotConnection);
        p->triggerAction(QWebEnginePage::Reload);
        return true;
    }

    if (c == "go.back") {
        if (!p->history()->canGoBack()) {
            srv->respond(client, id, false, "no history to go back");
            return true;
        }
        auto *timer = new QTimer(srv);
        timer->setSingleShot(true);
        timer->setInterval(15000);
        auto *conn = new QMetaObject::Connection();
        *conn = QObject::connect(p, &QWebEnginePage::loadingChanged, srv,
            [srv, client, id, timer, conn](const QWebEngineLoadingInfo &info) {
                if (info.status() != QWebEngineLoadingInfo::LoadSucceededStatus) return;
                timer->stop();
                timer->deleteLater();
                QObject::disconnect(*conn);
                delete conn;
                srv->respond(client, id, true, "back");
            });
        QObject::connect(timer, &QTimer::timeout, srv, [srv, client, id, conn, timer]() {
            QObject::disconnect(*conn);
            delete conn;
            timer->deleteLater();
            srv->respond(client, id, false, "go.back timeout");
        });
        timer->start();
        p->triggerAction(QWebEnginePage::Back);
        return true;
    }

    if (c == "go.forward") {
        if (!p->history()->canGoForward()) {
            srv->respond(client, id, false, "no history to go forward");
            return true;
        }
        auto *timer = new QTimer(srv);
        timer->setSingleShot(true);
        timer->setInterval(15000);
        auto *conn = new QMetaObject::Connection();
        *conn = QObject::connect(p, &QWebEnginePage::loadingChanged, srv,
            [srv, client, id, timer, conn](const QWebEngineLoadingInfo &info) {
                if (info.status() != QWebEngineLoadingInfo::LoadSucceededStatus) return;
                timer->stop();
                timer->deleteLater();
                QObject::disconnect(*conn);
                delete conn;
                srv->respond(client, id, true, "forward");
            });
        QObject::connect(timer, &QTimer::timeout, srv, [srv, client, id, conn, timer]() {
            QObject::disconnect(*conn);
            delete conn;
            timer->deleteLater();
            srv->respond(client, id, false, "go.forward timeout");
        });
        timer->start();
        p->triggerAction(QWebEnginePage::Forward);
        return true;
    }

    if (c == "page.url") {
        srv->respond(client, id, true, p->url().toString());
        return true;
    }

    if (c == "page.title") {
        srv->respond(client, id, true, p->title());
        return true;
    }

    if (c == "page.content") {
        p->toHtml([srv, client, id](const QString &html) {
            srv->respond(client, id, true, html);
        });
        return true;
    }

    if (c == "wait.navigation") {
        QObject::connect(p, &QWebEnginePage::loadFinished, srv,
            [srv, client, id](bool ok) {
                srv->respond(client, id, ok, ok ? "navigated" : "navigation failed");
            }, Qt::SingleShotConnection);
        return true;
    }

    if (c == "wait.selector") {
        QString selector = payload["selector"].toString();
        int timeout      = payload["timeout"].toInt(10000);
        struct PollState { int elapsed = 0; };
        auto *state = new PollState();
        auto *timer = new QTimer(srv);
        timer->setInterval(100);
        QObject::connect(timer, &QTimer::timeout, srv, [srv, client, id, p, selector, timeout, timer, state]() mutable {
            state->elapsed += 100;
            QString js = QString("(function(){ return !!document.querySelector('%1'); })()").arg(selector);
            p->runJavaScript(js, [srv, client, id, selector, timeout, timer, state](const QVariant &found) {
                if (found.toBool()) {
                    timer->stop(); timer->deleteLater(); delete state;
                    srv->respond(client, id, true, "found");
                } else if (state->elapsed >= timeout) {
                    timer->stop(); timer->deleteLater(); delete state;
                    srv->respond(client, id, false, "timeout waiting for selector: " + selector);
                }
            });
        });
        timer->start();
        return true;
    }

    if (c == "wait.response") {
        QTimer::singleShot(0, srv, [srv, client, id]() {
            srv->respond(client, id, true, "response ready");
        });
        return true;
    }

    return false; // not handled
}