#include "PiggyCaptcha.h"
#include "PiggyServer.h"
#include "../ProxyManager.h"
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QJsonArray>

QWebEnginePage* piggy_page(PiggyServer *srv, const QString &tabId);

// ─── Detection JS ─────────────────────────────────────────────────────────────
// Returns { captcha: string|null, blocked: string|null }
static const QString kDetectJs = R"JS(
(function() {
    var result = { captcha: null, blocked: null };
    var body   = (document.body ? document.body.innerHTML : '').toLowerCase();
    var title  = document.title.toLowerCase();
    var url    = window.location.href.toLowerCase();

    // ── Captcha detection ────────────────────────────────────────────────────

    // Cloudflare challenge (Turnstile or legacy)
    if (document.querySelector('#challenge-form, .cf-challenge-running, cf-turnstile, [id^=cf-challenge]')
        || body.indexOf('checking if the site connection is secure') !== -1
        || body.indexOf('just a moment') !== -1 && body.indexOf('cloudflare') !== -1) {
        result.captcha = 'cloudflare';
    }

    // reCAPTCHA
    if (!result.captcha &&
        (document.querySelector('.g-recaptcha, iframe[src*="recaptcha"]')
         || body.indexOf('recaptcha') !== -1)) {
        result.captcha = 'recaptcha';
    }

    // hCaptcha
    if (!result.captcha &&
        (document.querySelector('.h-captcha, iframe[src*="hcaptcha"]')
         || body.indexOf('hcaptcha') !== -1)) {
        result.captcha = 'hcaptcha';
    }

    // Generic captcha
    if (!result.captcha &&
        (body.indexOf('captcha') !== -1 || title.indexOf('captcha') !== -1)) {
        result.captcha = 'generic';
    }

    // ── Block / ban detection ────────────────────────────────────────────────

    var statusCode = 0;
    try {
        // performance.getEntriesByType gives us nav timing w/ responseStatus
        var nav = performance.getEntriesByType('navigation')[0];
        if (nav && nav.responseStatus) statusCode = nav.responseStatus;
    } catch(e) {}

    if (statusCode === 403 || statusCode === 429) {
        result.blocked = statusCode === 429 ? 'rate-limit' : '403';
    }

    if (!result.blocked &&
        (body.indexOf('access denied') !== -1
         || body.indexOf('you have been blocked') !== -1
         || body.indexOf('your ip has been banned') !== -1
         || body.indexOf('403 forbidden') !== -1)) {
        result.blocked = 'access-denied';
    }

    if (!result.blocked &&
        (body.indexOf('cloudflare') !== -1 && body.indexOf('sorry') !== -1
         || body.indexOf('ray id:') !== -1 && !result.captcha)) {
        result.blocked = 'firewall';
    }

    if (!result.blocked &&
        (body.indexOf('too many requests') !== -1
         || body.indexOf('rate limit') !== -1)) {
        result.blocked = 'rate-limit';
    }

    return result;
})()
)JS";

// ─── Singleton ────────────────────────────────────────────────────────────────
static PiggyCaptchaDetector *s_detector = nullptr;

PiggyCaptchaDetector *piggy_captchaDetector() { return s_detector; }
void piggy_captchaDetectorInit(PiggyServer *srv) {
    if (!s_detector) s_detector = new PiggyCaptchaDetector(srv, srv);
}

// ─── PiggyCaptchaDetector ─────────────────────────────────────────────────────

PiggyCaptchaDetector::PiggyCaptchaDetector(PiggyServer *srv, QObject *parent)
    : QObject(parent), m_srv(srv) {}

void PiggyCaptchaDetector::watchTab(const QString &tabId, QWebEnginePage *page) {
    m_pages[tabId] = page;
    m_captcha[tabId] = CaptchaState{};
    m_block[tabId]   = BlockState{};

    QObject::connect(page, &QWebEnginePage::loadFinished, this,
        [this, tabId](bool ok) {
            onLoadFinished(ok, tabId);
        });
}

void PiggyCaptchaDetector::unwatchTab(const QString &tabId) {
    m_pages.remove(tabId);
    m_captcha.remove(tabId);
    m_block.remove(tabId);
}

void PiggyCaptchaDetector::onLoadFinished(bool /*ok*/, const QString &tabId) {
    runDetection(tabId);
}

void PiggyCaptchaDetector::runDetection(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) return;

    page->runJavaScript(kDetectJs,
        [this, tabId](const QVariant &r) {
            QVariantMap map = r.toMap();
            QString captchaType = map["captcha"].toString();
            QString blockedType = map["blocked"].toString();

            bool captchaFound = !captchaType.isEmpty() && captchaType != "null";
            bool blockedFound = !blockedType.isEmpty() && blockedType != "null";

            // ── Captcha ───────────────────────────────────────────────────────
            if (captchaFound && !m_captcha[tabId].detected) {
                m_captcha[tabId].detected = true;
                m_captcha[tabId].paused   = true;
                m_captcha[tabId].type     = captchaType;

                QJsonObject ev;
                ev["type"]        = "event";
                ev["event"]       = "captcha";
                ev["tabId"]       = tabId;
                ev["captchaType"] = captchaType;
                broadcastEvent(ev);

                qDebug() << "[PiggyCaptcha] Captcha detected on tab" << tabId << "type:" << captchaType;
            }

            // ── Block ─────────────────────────────────────────────────────────
            if (blockedFound && !m_block[tabId].detected) {
                m_block[tabId].detected = true;
                m_block[tabId].type     = blockedType;

                QJsonObject ev;
                ev["type"]       = "event";
                ev["event"]      = "blocked";
                ev["tabId"]      = tabId;
                ev["blockType"]  = blockedType;
                broadcastEvent(ev);

                qDebug() << "[PiggyCaptcha] Block detected on tab" << tabId << "type:" << blockedType;

                if (m_autoRetry) {
                    QTimer::singleShot(500, this, [this, tabId]() {
                        blockRetry(tabId);
                    });
                }
            }

            // Reset if page no longer shows captcha/block
            if (!captchaFound && m_captcha[tabId].detected) {
                m_captcha[tabId] = CaptchaState{};
            }
            if (!blockedFound && m_block[tabId].detected) {
                m_block[tabId] = BlockState{};
            }
        });
}

void PiggyCaptchaDetector::resolveTab(const QString &tabId) {
    if (!m_captcha.contains(tabId)) return;
    m_captcha[tabId].detected = false;
    m_captcha[tabId].paused   = false;
    m_captcha[tabId].type.clear();

    QJsonObject ev;
    ev["type"]  = "event";
    ev["event"] = "captcha:resolved";
    ev["tabId"] = tabId;
    broadcastEvent(ev);

    qDebug() << "[PiggyCaptcha] Captcha resolved on tab" << tabId;
}

void PiggyCaptchaDetector::pauseTab(const QString &tabId) {
    if (!m_captcha.contains(tabId)) return;
    m_captcha[tabId].paused = true;
}

void PiggyCaptchaDetector::forceCheck(const QString &tabId) {
    runDetection(tabId);
}

void PiggyCaptchaDetector::blockRetry(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) return;

    // Rotate proxy
    auto &pm = ProxyManager::instance();
    if (pm.count() > 0) {
        pm.next();
        qDebug() << "[PiggyCaptcha] Auto-rotated proxy to:" << pm.current().toString();
    }

    // Reset block state
    m_block[tabId] = BlockState{};

    // Reload page
    QObject::connect(page, &QWebEnginePage::loadFinished, this,
        [this, tabId](bool) {
            runDetection(tabId);
        }, Qt::SingleShotConnection);
    page->triggerAction(QWebEnginePage::Reload);

    QJsonObject ev;
    ev["type"]  = "event";
    ev["event"] = "block:retry";
    ev["tabId"] = tabId;
    ev["proxy"] = pm.current().toString();
    broadcastEvent(ev);
}

void PiggyCaptchaDetector::broadcastEvent(const QJsonObject &event) {
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_srv->clients()) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleCaptcha(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId)
{
    Q_UNUSED(srv);

    auto *det = piggy_captchaDetector();
    if (!det) {
        srv->respond(client, id, false, "captcha detector not initialized");
        return c.startsWith("captcha.") || c.startsWith("block.");
    }

    if (c == "captcha.status") {
        CaptchaState s = det->captchaState(tabId);
        QJsonObject o;
        o["detected"] = s.detected;
        o["paused"]   = s.paused;
        o["type"]     = s.type;
        srv->respond(client, id, true, o);
        return true;
    }

    if (c == "captcha.resolve") {
        det->resolveTab(tabId);
        srv->respond(client, id, true, "captcha resolved");
        return true;
    }

    if (c == "captcha.pause") {
        det->pauseTab(tabId);
        srv->respond(client, id, true, "tab paused");
        return true;
    }

    if (c == "captcha.check") {
        det->forceCheck(tabId);
        srv->respond(client, id, true, "detection running");
        return true;
    }

    if (c == "captcha.autoRetry") {
        bool on = payload["enabled"].toBool(true);
        det->setAutoRetry(on);
        srv->respond(client, id, true, on ? "autoRetry enabled" : "autoRetry disabled");
        return true;
    }

    if (c == "block.status") {
        BlockState s = det->blockState(tabId);
        QJsonObject o;
        o["detected"] = s.detected;
        o["type"]     = s.type;
        srv->respond(client, id, true, o);
        return true;
    }

    if (c == "block.retry") {
        det->blockRetry(tabId);
        srv->respond(client, id, true, "retry initiated");
        return true;
    }

    return false;
}