#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QWebEnginePage>
#include <QWebEngineProfile>

// Forward declare — headful mode gets a real PiggyTab pointer
class PiggyTab;

class PiggyServer : public QObject {
    Q_OBJECT
public:
    // headful: pass piggy tab pointer
    // headless: pass nullptr — server creates its own page
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
                      const QString &reqId);
    QWebEnginePage* page();

    PiggyTab         *m_piggy   = nullptr;  // null in headless
    QLocalServer     *m_server  = nullptr;
    QWebEnginePage   *m_ownPage = nullptr;  // headless only
    QWebEngineProfile*m_ownProfile = nullptr;

    QList<QLocalSocket*> m_clients;

    static constexpr char SOCKET_NAME[] = "piggy";
};
