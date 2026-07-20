#pragma once
#include <QObject>
#include <QWebSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "NetworkCapture.h"   // for WebSocketFrame

class CdpProbe : public QObject {
    Q_OBJECT
public:
    explicit CdpProbe(QObject *parent = nullptr);
    void start(const QString &debugHost = "127.0.0.1", int debugPort = 9222);
    void stop();   // optional, for cleanup

signals:
    void wsFrameCaptured(const WebSocketFrame &frame);

private slots:
    void onCdpConnected();
    void onCdpMessage(const QString &message);

private:
    QNetworkAccessManager *m_nam = nullptr;
    QWebSocket            *m_ws  = nullptr;
    int                    m_nextId = 1;
    QString                m_currentRequestId;  // track current WS requestId

    void pickBestTarget(const QByteArray &json, const QString &host, int port);
    void refreshTargets();   // periodic polling
};