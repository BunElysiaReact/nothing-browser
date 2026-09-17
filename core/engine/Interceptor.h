#pragma once
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include "blocker.h"
#include "request.h"

class Interceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit Interceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    // Toggle adblock on/off at runtime — defaults to enabled.
    void setAdblockEnabled(bool enabled) { m_adblockEnabled = enabled; }
    bool adblockEnabled() const          { return m_adblockEnabled; }

signals:
    // Fired for every request that makes it past the blockers.
    void requestSeen(const QString &method,
                     const QString &url,
                     const QString &headers);

    // Fired for every intercepted (non-XHR) request, with its resolved type.
    void requestIntercepted(const QString &method,
                            const QString &url,
                            const QString &type,
                            const QString &headers);

private:
    // Converts Qt resource-type enum to the adblock library's type enum.
    static adblock::RequestType toAdblockType(int resourceType);

    adblock::Blocker m_blocker;
    bool             m_adblockEnabled = true;
};