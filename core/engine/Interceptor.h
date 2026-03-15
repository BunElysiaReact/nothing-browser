#pragma once
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

class Interceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit Interceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
signals:
    void requestSeen(const QString &method,
                     const QString &url,
                     const QString &headers);
};