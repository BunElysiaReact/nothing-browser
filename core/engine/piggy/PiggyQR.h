#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMap>
#include <QTimer>

class PiggyServer;
class QWebEnginePage;

// ─── QR Code detector ─────────────────────────────────────────────────────────
// Watches for WAWeb's QR canvas to appear, extracts base64 PNG,
// emits event to Node AND prints ASCII QR to stdout.
//
// Events emitted:
//   { type:"event", event:"qr", tabId, qrData:"data:image/png;base64,..." }
//   { type:"event", event:"qr:scanned", tabId }   — when QR disappears (auth done)
//   { type:"event", event:"qr:timeout", tabId }   — QR expired, new one incoming
//
// Commands:
//   qr.status   { tabId } → { waiting:bool, attempts:int }
//   qr.force    { tabId } → force re-check immediately
// ─────────────────────────────────────────────────────────────────────────────

struct QRState {
    bool    waiting  = false;
    int     attempts = 0;
    QString lastHash;         // detect when QR rotates
};

class PiggyQRDetector : public QObject {
    Q_OBJECT
public:
    explicit PiggyQRDetector(PiggyServer *srv, QObject *parent = nullptr);

    void watchTab(const QString &tabId, QWebEnginePage *page);
    void unwatchTab(const QString &tabId);
    void forceCheck(const QString &tabId);

    QRState qrState(const QString &tabId) const { return m_states.value(tabId); }

private slots:
    void onLoadFinished(bool ok, const QString &tabId);

private:
    void startPolling(const QString &tabId);
    void stopPolling(const QString &tabId);
    void runDetection(const QString &tabId);
    void extractQR(const QString &tabId);
    void printAsciiQR(const QString &base64png);
    void broadcastEvent(const QJsonObject &event);

    PiggyServer                    *m_srv;
    QMap<QString, QRState>          m_states;
    QMap<QString, QWebEnginePage*>  m_pages;
    QMap<QString, QTimer*>          m_timers;
};

bool piggy_handleQR(PiggyServer *srv, const QString &c,
                    const QJsonObject &payload,
                    QLocalSocket *client, const QString &id,
                    const QString &tabId);

PiggyQRDetector *piggy_qrDetector();
void             piggy_qrDetectorInit(PiggyServer *srv);