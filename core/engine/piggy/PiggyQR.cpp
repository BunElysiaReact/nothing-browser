#include "PiggyQR.h"
#include "PiggyServer.h"
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QTimer>
#include <QCryptographicHash>

// WAWeb QR selectors — it rotates between a few depending on version
static const QStringList kQRSelectors = {
    "canvas[aria-label='Scan this QR code to link a device']",
    "div[data-ref] canvas",
    "canvas.landing-main-qr",
    "canvas"   // fallback — only fires if others miss
};

// How often to poll for QR canvas (ms)
static const int kPollInterval = 500;

// ─── ASCII QR printer ─────────────────────────────────────────────────────────
// We can't decode PNG in C++ without a lib, so we print the raw base64
// as a url and use a simple JS-side trick — OR we emit the event and
// also write a small helper. For terminal printing we use iterm2/kitty
// inline image protocol as primary, with a fallback url.
void PiggyQRDetector::printAsciiQR(const QString &base64png) {
    // Try iTerm2 / Kitty inline image protocol first
    // This shows actual QR image inline in supported terminals
    QString iterm = QString("\033]1337;File=inline=1;width=20;height=20:%1\007")
        .arg(base64png);
    fprintf(stdout, "%s\n", iterm.toUtf8().constData());

    // Kitty protocol fallback
    QString kitty = QString("\033_Ga=T,f=100,m=0;%1\033\\")
        .arg(base64png);
    fprintf(stdout, "%s\n", kitty.toUtf8().constData());

    // Plain fallback for dumb terminals — just print the data URL
    // Node side can render this with qrcode-terminal if they want
    fprintf(stdout, "\n[Piggy QR] Scan this QR code in WhatsApp:\n");
    fprintf(stdout, "data:image/png;base64,%s\n\n",
            base64png.toUtf8().constData());
    fflush(stdout);
}

// ─── Singleton ────────────────────────────────────────────────────────────────
static PiggyQRDetector *s_qrDetector = nullptr;
PiggyQRDetector *piggy_qrDetector() { return s_qrDetector; }
void piggy_qrDetectorInit(PiggyServer *srv) {
    if (!s_qrDetector) s_qrDetector = new PiggyQRDetector(srv, srv);
}

// ─── PiggyQRDetector ──────────────────────────────────────────────────────────

PiggyQRDetector::PiggyQRDetector(PiggyServer *srv, QObject *parent)
    : QObject(parent), m_srv(srv) {}

void PiggyQRDetector::watchTab(const QString &tabId, QWebEnginePage *page) {
    m_pages[tabId]  = page;
    m_states[tabId] = QRState{};

    QObject::connect(page, &QWebEnginePage::loadFinished, this,
        [this, tabId](bool ok) {
            onLoadFinished(ok, tabId);
        });
}

void PiggyQRDetector::unwatchTab(const QString &tabId) {
    stopPolling(tabId);
    m_pages.remove(tabId);
    m_states.remove(tabId);
}

void PiggyQRDetector::onLoadFinished(bool ok, const QString &tabId) {
    if (!ok) return;
    // Start polling for QR after page loads
    startPolling(tabId);
}

void PiggyQRDetector::startPolling(const QString &tabId) {
    if (m_timers.contains(tabId)) return; // already polling

    auto *timer = new QTimer(this);
    timer->setInterval(kPollInterval);
    m_timers[tabId] = timer;

    QObject::connect(timer, &QTimer::timeout, this,
        [this, tabId]() { runDetection(tabId); });
    timer->start();
}

void PiggyQRDetector::stopPolling(const QString &tabId) {
    if (!m_timers.contains(tabId)) return;
    m_timers[tabId]->stop();
    m_timers[tabId]->deleteLater();
    m_timers.remove(tabId);
}

void PiggyQRDetector::runDetection(const QString &tabId) {
    auto *page = m_pages.value(tabId, nullptr);
    if (!page) return;

    // Try each selector until we find the canvas
    // Build a JS that tries all selectors and returns the first hit
    QString selectorsJs;
    for (const QString &sel : kQRSelectors) {
        selectorsJs += QString("document.querySelector('%1') || ").arg(sel);
    }
    selectorsJs += "null";

    QString js = QString(
        "(function(){"
        "  var canvas = %1;"
        "  if (!canvas || canvas.tagName.toLowerCase() !== 'canvas') return null;"
        "  if (canvas.width === 0 || canvas.height === 0) return null;"
        "  return canvas.toDataURL('image/png').split(',')[1];"
        "})()"
    ).arg(selectorsJs);

    page->runJavaScript(js, [this, tabId](const QVariant &r) {
        if (!m_states.contains(tabId)) return;

        if (r.isNull() || !r.isValid() || r.toString().isEmpty()) {
            // Canvas gone — if we were waiting, QR was scanned
            if (m_states[tabId].waiting) {
                m_states[tabId].waiting = false;
                stopPolling(tabId);

                QJsonObject ev;
                ev["type"]  = "event";
                ev["event"] = "qr:scanned";
                ev["tabId"] = tabId;
                broadcastEvent(ev);
                qDebug() << "[PiggyQR] QR scanned on tab" << tabId;
            }
            return;
        }

        QString base64 = r.toString();

        // Hash to detect QR rotation (WAWeb rotates every ~20s)
        QString hash = QString::fromLatin1(
            QCryptographicHash::hash(base64.toUtf8(),
                QCryptographicHash::Md5).toHex());

        if (hash == m_states[tabId].lastHash) return; // same QR, skip

        // New QR (or rotated QR)
        bool wasWaiting = m_states[tabId].waiting;
        m_states[tabId].waiting  = true;
        m_states[tabId].attempts++;
        m_states[tabId].lastHash = hash;

        if (wasWaiting) {
            // QR rotated
            QJsonObject ev;
            ev["type"]  = "event";
            ev["event"] = "qr:timeout";
            ev["tabId"] = tabId;
            broadcastEvent(ev);
            qDebug() << "[PiggyQR] QR rotated on tab" << tabId;
        }

        // Emit event to Node
        QJsonObject ev;
        ev["type"]    = "event";
        ev["event"]   = "qr";
        ev["tabId"]   = tabId;
        ev["qrData"]  = "data:image/png;base64," + base64;
        ev["attempts"]= m_states[tabId].attempts;
        broadcastEvent(ev);

        // Print to terminal
        printAsciiQR(base64);

        qDebug() << "[PiggyQR] QR emitted on tab" << tabId
                 << "attempt" << m_states[tabId].attempts;
    });
}

void PiggyQRDetector::forceCheck(const QString &tabId) {
    runDetection(tabId);
}

void PiggyQRDetector::broadcastEvent(const QJsonObject &event) {
    QByteArray msg = QJsonDocument(event).toJson(QJsonDocument::Compact) + "\n";
    for (auto *client : m_srv->clients()) {
        if (client && client->state() == QLocalSocket::ConnectedState)
            client->write(msg);
    }
}

// ─── Command handler ──────────────────────────────────────────────────────────

bool piggy_handleQR(PiggyServer *srv, const QString &c,
                    const QJsonObject &payload,
                    QLocalSocket *client, const QString &id,
                    const QString &tabId)
{
    auto *det = piggy_qrDetector();

    if (c == "qr.status") {
        if (!det) {
            srv->respond(client, id, true,
                QJsonObject{{"waiting", false}, {"attempts", 0}});
            return true;
        }
        QRState s = det->qrState(tabId);
        QJsonObject o;
        o["waiting"]  = s.waiting;
        o["attempts"] = s.attempts;
        srv->respond(client, id, true, o);
        return true;
    }

    if (c == "qr.force") {
        if (!det) {
            srv->respond(client, id, false, "qr detector not initialized");
            return true;
        }
        det->forceCheck(tabId);
        srv->respond(client, id, true, "qr check triggered");
        return true;
    }

    return false;
}