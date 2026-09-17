#pragma once
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include "blocker.h"
#include "request.h"

class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit RequestInterceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    void setAdblockEnabled(bool enabled) { m_adblockEnabled = enabled; }
signals:
    void requestIntercepted(QString method, QString url, QString type, QString headers);
private:
    adblock::Blocker m_blocker;
    bool             m_adblockEnabled = true;
};