#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineUrlRequestInterceptor>
#include <QNetworkCookie>
#include <QMap>
#include <QVector>
#include <QList>
#include <QUuid>

class PiggyTab;
class NetworkCapture;
class Interceptor;
struct CapturedRequest;
struct WebSocketFrame;
struct CapturedCookie;

struct InterceptRule {
    QString urlPattern;
    bool block = false;
    QString redirectUrl;
    QMap<QString, QString> setHeaders;
    QMap<QString, QString> removeHeaders;
};

class PiggyServer : public QObject {
    Q_OBJECT
public:
    explicit PiggyServer(PiggyTab *piggy, QObject *parent = nullptr);
    ~PiggyServer();

    void start();
    void stop();

private slots:
    void onNewConnection();
    void onClientData();
    void onClientDisconnected();
    void onRequestCaptured(const CapturedRequest &req, const QString &tabId);
    void onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId);
    void onCookieCaptured(const CapturedCookie &cookie, const QString &tabId);
    void onCookieRemoved(const QString &name, const QString &domain, const QString &tabId);
    void onStorageCaptured(const QString &origin, const QString &key,
                           const QString &value, const QString &storageType, const QString &tabId);

private:
    void handleCommand(const QJsonObject &cmd, QLocalSocket *client);
    void respond(QLocalSocket *client, const QString &id,
                 bool ok, const QVariant &data = QVariant());
    void navigatePage(const QString &url, QLocalSocket *client,
                      const QString &reqId, const QString &tabId);

    QWebEnginePage* page(const QString &tabId = QString());
    QString createTab();
    void closeTab(const QString &tabId);

    void doScreenshot(QLocalSocket *client, const QString &id, const QString &tabId);
    void doPdf(QLocalSocket *client, const QString &id, const QString &tabId);
    void setImageBlocking(const QString &tabId, bool block);

    QList<QNetworkCookie> cookiesForTab(const QString &tabId);

    void applyInterceptRules(Interceptor *interceptor, const QVector<InterceptRule> &rules);
    QVector<InterceptRule> m_interceptRules;

    void startCapture(const QString &tabId);
    void stopCapture(const QString &tabId);
    bool isCapturing(const QString &tabId) const;

    PiggyTab              *m_piggy      = nullptr;
    QLocalServer          *m_server     = nullptr;

    QWebEngineProfile     *m_ownProfile = nullptr;
    QWebEnginePage        *m_ownPage    = nullptr;

    struct TabContext {
        QWebEnginePage   *page        = nullptr;
        Interceptor      *interceptor = nullptr;
        NetworkCapture   *capture     = nullptr;
        bool              imageBlocked  = false;
        bool              captureActive = false;
        QVector<InterceptRule>           rules;
        QList<CapturedRequest>           capturedRequests;
        QList<WebSocketFrame>            capturedWsFrames;
        QList<CapturedCookie>            capturedCookies;
        QList<QPair<QString, QString>>   storageEntries;
    };
    QMap<QString, TabContext> m_tabs;

    QList<QLocalSocket*>   m_clients;

    static constexpr char SOCKET_NAME[] = "piggy";
};