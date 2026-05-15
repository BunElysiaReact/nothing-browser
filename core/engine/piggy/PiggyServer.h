#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTcpSocket>
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
#include <QDir>
#include <QSet>
#include "../NetworkCapture.h"

class PiggyTab;
class Interceptor;
class SessionManager;

struct InterceptRule {
    QString urlPattern;
    bool block = false;
    QString redirectUrl;
    QMap<QString, QString> setHeaders;
    QMap<QString, QString> removeHeaders;
};

struct TabContext {
    QWebEnginePage   *page             = nullptr;
    Interceptor      *interceptor      = nullptr;
    NetworkCapture   *capture          = nullptr;
    QStringList       initScripts;
    bool              imageBlocked     = false;
    bool              captureActive    = false;
    bool              exposedConnected = false;
    QStringList       exposedFunctions;
    QVector<InterceptRule>           rules;
    QList<CapturedRequest>           capturedRequests;
    QList<WebSocketFrame>            capturedWsFrames;
    QList<CapturedCookie>            capturedCookies;
    QList<QPair<QString, QString>>   storageEntries;
};

class PiggyServer : public QObject {
    Q_OBJECT
public:
    explicit PiggyServer(PiggyTab *piggy, QObject *parent = nullptr);
    explicit PiggyServer(QWebEnginePage *page, QObject *parent = nullptr);
    ~PiggyServer();

    void start();
    void stop();
    void startHttp(const QString &apiKey);

    QMap<QString, TabContext>  &tabs()        { return m_tabs; }
    QList<QLocalSocket*>       &clients()     { return m_clients; }
    PiggyTab                   *piggy()       { return m_piggy; }
    QWebEnginePage             *headfulPage() { return m_headfulPage; }
    QWebEngineProfile          *ownProfile()  { return m_ownProfile; }
    QWebEnginePage             *ownPage()     { return m_ownPage; }
    SessionManager             *session()     { return m_session; }

    void respond(QLocalSocket *client, const QString &id,
                 bool ok, const QVariant &data = QVariant());

    void handleHttpCommand(const QJsonObject &cmd, QTcpSocket *sock);
    void respondHttp(QTcpSocket *sock, const QString &id,
                     bool ok, const QVariant &data = QVariant());

    QWebEnginePage* page(const QString &tabId = QString());
    QString         createTab();
    void            closeTab(const QString &tabId);

    static constexpr char SOCKET_NAME[] = "piggy";

signals:
    void tabCreated(const QString &tabId, QWebEnginePage *page);
    void tabClosed(const QString &tabId);

public slots:
    void onRequestCaptured(const CapturedRequest &req, const QString &tabId);
    void onWsFrameCaptured(const WebSocketFrame &frame, const QString &tabId);
    void onCookieCaptured(const CapturedCookie &cookie, const QString &tabId);
    void onCookieRemoved(const QString &name, const QString &domain, const QString &tabId);
    void onStorageCaptured(const QString &origin, const QString &key,
                           const QString &value, const QString &storageType,
                           const QString &tabId);
    void onExposedFunctionCalled(const QString &name, const QString &callId,
                                 const QString &data, const QString &tabId);

private slots:
    void onNewConnection();
    void onClientData();
    void onClientDisconnected();

private:
    void handleCommand(const QJsonObject &cmd, QLocalSocket *client);

    PiggyTab          *m_piggy           = nullptr;
    QWebEnginePage    *m_headfulPage     = nullptr;
    QLocalServer      *m_server          = nullptr;
    QWebEngineProfile *m_ownProfile      = nullptr;
    QWebEnginePage    *m_ownPage         = nullptr;
    SessionManager    *m_session         = nullptr;
    QString            m_apiKey;
    QTcpSocket        *m_pendingHttpSock = nullptr;

    QMap<QString, TabContext>       m_tabs;
    QList<QLocalSocket*>            m_clients;
    QMap<QLocalSocket*, QByteArray> m_buffers;  // line buffer per client
};