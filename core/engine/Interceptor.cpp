#include "Interceptor.h"
#include <QDateTime>

Interceptor::Interceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent) {}

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    info.setHttpHeader("User-Agent",
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36");
    info.setHttpHeader("Accept-Language", "en-US,en;q=0.9");
    info.setHttpHeader("Accept-Encoding", "gzip, deflate, br");
    info.setHttpHeader("DNT", "1");

    auto type = info.resourceType();
    bool isJunk = (type == QWebEngineUrlRequestInfo::ResourceTypeFontResource ||
                   type == QWebEngineUrlRequestInfo::ResourceTypeMedia);
    if (isJunk) { info.block(true); return; }

    QString method  = QString::fromLatin1(info.requestMethod());
    QString url     = info.requestUrl().toString();
    QString headers = QString("Method: %1\nURL: %2\nType: %3\nTime: %4")
                        .arg(method).arg(url)
                        .arg(static_cast<int>(type))
                        .arg(QDateTime::currentDateTime()
                                 .toString("hh:mm:ss.zzz"));
    emit requestSeen(method, url, headers);
}