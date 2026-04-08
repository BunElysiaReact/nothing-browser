#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QMap>
#include <QUuid>

class PiggyTab;

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

private:
    void handleCommand(const QJsonObject &cmd, QLocalSocket *client);
    void respond(QLocalSocket *client, const QString &id,
                 bool ok, const QVariant &data = QVariant());
    void navigatePage(const QString &url, QLocalSocket *client,
                      const QString &reqId, const QString &tabId);

    QWebEnginePage* page(const QString &tabId = QString());
    QString createTab();
    void closeTab(const QString &tabId);

    PiggyTab              *m_piggy      = nullptr;
    QLocalServer          *m_server     = nullptr;

    // headless-mode own profile + fallback page
    QWebEngineProfile     *m_ownProfile = nullptr;
    QWebEnginePage        *m_ownPage    = nullptr;

    // headless multi-tab pool
    QMap<QString, QWebEnginePage*> m_tabs;

    QList<QLocalSocket*>   m_clients;

    static constexpr char SOCKET_NAME[] = "piggy";
};