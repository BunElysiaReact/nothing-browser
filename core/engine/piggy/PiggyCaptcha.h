#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QMap>

class PiggyServer;
class QWebEnginePage;

// ─── Captcha + Block detection engine ────────────────────────────────────────
// On load-finished of every page, PiggyCaptchaDetector runs a JS heuristic
// to detect:
//   - CAPTCHA presence (Cloudflare challenge, reCAPTCHA, hCaptcha, generic)
//   - Ban/block signals (403, access-denied text, firewall pages)
//
// On CAPTCHA detection:
//   1. Emits "captcha" event to all JS clients (with type + tabId)
//   2. Pauses the tab (navigation blocked until captcha.resolve called)
//   3. DOES NOT auto-rotate proxy — caller decides
//
// On BLOCK detection:
//   1. Emits "blocked" event to all JS clients
//   2. Auto-rotates proxy via ProxyManager::next()
//   3. Reloads page with new proxy (if autoRetry is on)
//
// Commands:
//   captcha.status     { tabId }       → { detected:bool, type:string }
//   captcha.resolve    { tabId }       → unpauses tab, emits captcha:resolved
//   captcha.pause      { tabId }       → manually pause tab
//   captcha.check      { tabId }       → force immediate detection check
//   captcha.autoRetry  { enabled }     → toggle auto-proxy-rotate on block
//   block.status       { tabId }       → { blocked:bool, type:string }
//   block.retry        { tabId }       → rotate proxy + reload
// ─────────────────────────────────────────────────────────────────────────────

struct CaptchaState {
    bool    detected   = false;
    bool    paused     = false;
    QString type;       // "cloudflare" | "recaptcha" | "hcaptcha" | "generic"
};

struct BlockState {
    bool    detected   = false;
    QString type;       // "403" | "access-denied" | "firewall" | "rate-limit"
};

class PiggyCaptchaDetector : public QObject {
    Q_OBJECT
public:
    explicit PiggyCaptchaDetector(PiggyServer *srv, QObject *parent = nullptr);

    // Wire to a page — call once per tab after page creation
    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);

    // Toggle auto proxy-rotate + reload on block
    void setAutoRetry(bool on) { m_autoRetry = on; }
    bool autoRetry() const { return m_autoRetry; }

    CaptchaState captchaState(const QString &tabId) const { return m_captcha.value(tabId); }
    BlockState   blockState(const QString &tabId)   const { return m_block.value(tabId); }

    void resolveTab(const QString &tabId);
    void pauseTab(const QString &tabId);
    void forceCheck(const QString &tabId);
    void blockRetry(const QString &tabId);

private slots:
    void onLoadFinished(bool ok, const QString &tabId);

private:
    void runDetection(const QString &tabId);
    void broadcastEvent(const QJsonObject &event);

    PiggyServer                    *m_srv;
    bool                            m_autoRetry = true;
    QMap<QString, CaptchaState>     m_captcha;
    QMap<QString, BlockState>       m_block;
    QMap<QString, QWebEnginePage*>  m_pages;
};

// ─── Free command handler (registered in PiggyCommandRouter) ─────────────────
bool piggy_handleCaptcha(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId);

// ─── Global singleton accessor (created once in PiggyServer) ─────────────────
PiggyCaptchaDetector *piggy_captchaDetector();
void                  piggy_captchaDetectorInit(PiggyServer *srv);