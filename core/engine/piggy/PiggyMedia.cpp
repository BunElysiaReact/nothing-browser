#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QPageLayout>
#include <QPageSize>
#include <QMarginsF>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Screenshot ───────────────────────────────────────────────────────────────

void piggy_doScreenshot(PiggyServer *srv, QLocalSocket *client,
                         const QString &id, const QString &tabId) {
    auto *p = piggy_page(srv, tabId);
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
    p->runJavaScript(js, [srv, client, id](const QVariant &r) {
        if (r.isNull() || !r.isValid())
            srv->respond(client, id, false, "screenshot: JS render failed");
        else
            srv->respond(client, id, true, r.toString());
    });
}

// ─── PDF ─────────────────────────────────────────────────────────────────────

void piggy_doPdf(PiggyServer *srv, QLocalSocket *client,
                  const QString &id, const QString &tabId) {
    auto *p = piggy_page(srv, tabId);
    QPageLayout layout(QPageSize(QPageSize::A4),
                       QPageLayout::Portrait,
                       QMarginsF(15, 15, 15, 15),
                       QPageLayout::Millimeter);
    p->printToPdf([srv, client, id](const QByteArray &pdfData) {
        if (pdfData.isEmpty()) {
            srv->respond(client, id, false, "pdf: render failed");
            return;
        }
        srv->respond(client, id, true, QString::fromLatin1(pdfData.toBase64()));
    }, layout);
}

// ─── Image blocking ───────────────────────────────────────────────────────────

void piggy_setImageBlocking(PiggyServer *srv, const QString &tabId, bool block) {
    auto *p = piggy_page(srv, tabId);
    p->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, !block);
    if (srv->tabs().contains(tabId))
        srv->tabs()[tabId].imageBlocked = block;
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleMedia(PiggyServer *srv, const QString &c,
                        const QJsonObject & /*payload*/,
                        QLocalSocket *client, const QString &id,
                        const QString &tabId) {
    if (c == "screenshot") {
        piggy_doScreenshot(srv, client, id, tabId);
        return true;
    }
    if (c == "pdf") {
        piggy_doPdf(srv, client, id, tabId);
        return true;
    }
    if (c == "intercept.block.images") {
        piggy_setImageBlocking(srv, tabId, true);
        srv->respond(client, id, true, "images blocked");
        return true;
    }
    if (c == "intercept.unblock.images") {
        piggy_setImageBlocking(srv, tabId, false);
        srv->respond(client, id, true, "images unblocked");
        return true;
    }
    return false;
}