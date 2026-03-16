#pragma once
#include <QObject>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QNetworkCookie>
#include <QVariantMap>
#include <QDateTime>

struct CapturedRequest {
    QString id;
    QString method;
    QString url;
    QString type;       // XHR, Fetch, WS, Doc, Script, Img, etc.
    QString status;
    QString mimeType;
    QString requestHeaders;
    QString responseHeaders;
    QString responseBody;   // from JS interception (XHR/fetch)
    QString timestamp;
    qint64  size = 0;
    bool    isWebSocket = false;
};

struct WebSocketFrame {
    QString connectionId;
    QString url;
    QString direction;  // "SENT" | "RECV"
    QString data;
    QString timestamp;
    bool    isBinary = false;
};

struct CapturedCookie {
    QString name;
    QString value;
    QString domain;
    QString path;
    bool    httpOnly = false;
    bool    secure   = false;
    QString expires;
    QString sameSite;
};

Q_DECLARE_METATYPE(CapturedRequest)
Q_DECLARE_METATYPE(WebSocketFrame)

class NetworkCapture : public QObject {
    Q_OBJECT
public:
    explicit NetworkCapture(QObject *parent = nullptr);

    // Call once after BrowserTab sets up its QWebEnginePage
    void attachToPage(QWebEnginePage *page, QWebEngineProfile *profile);

    // JS to inject — intercepts fetch + XHR + WebSocket at page level
    static QString captureScript();
    // JS to inject into WA's Web Worker
    static QString workerCaptureScript();

signals:
    void requestCaptured(const CapturedRequest &req);
    void wsFrameCaptured(const WebSocketFrame &frame);
    void cookieCaptured(const CapturedCookie &cookie);
    void cookieRemoved(const QString &name, const QString &domain);
    void storageCaptured(const QString &origin, const QString &key,
                         const QString &value, const QString &storageType);

private slots:
    void onJsMessage(const QString &json);
    void onCookieAdded(const QNetworkCookie &cookie);
    void onCookieRemoved(const QNetworkCookie &cookie);

private:
    QWebEnginePage    *m_page    = nullptr;
    QWebEngineProfile *m_profile = nullptr;
};