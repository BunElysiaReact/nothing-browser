#include "UpdateChecker.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    m_nam   = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);

    connect(m_nam,   &QNetworkAccessManager::finished,
            this,    &UpdateChecker::onReply);
    connect(m_timer, &QTimer::timeout,
            this,    &UpdateChecker::checkNow);

    // Check on startup after 3s delay, then every 6 hours
    QTimer::singleShot(3000, this, &UpdateChecker::checkNow);
    m_timer->start(6 * 60 * 60 * 1000);
}

void UpdateChecker::checkNow() {
    QNetworkRequest req{QUrl(VERSION_URL)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "NothingBrowser/" + QString(CURRENT_VERSION));
    m_nam->get(req);
}

void UpdateChecker::onReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }
    VersionInfo info = parseJson(reply->readAll());
    info.isNewer     = versionToInt(info.version) > versionToInt(CURRENT_VERSION);
    if (info.isNewer) emit updateAvailable(info);
    else              emit noUpdate(info);
}

int UpdateChecker::versionToInt(const QString &v) {
    // "1.2.3" → 10203  so "0.2.0" > "0.1.9" works
    auto parts = v.split(".");
    while (parts.size() < 3) parts << "0";
    return parts[0].toInt() * 10000
         + parts[1].toInt() * 100
         + parts[2].toInt();
}

VersionInfo UpdateChecker::parseJson(const QByteArray &data) {
    VersionInfo info;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return info;
    QJsonObject obj = doc.object();
    info.version     = obj["version"].toString();
    info.downloadUrl = obj["url"].toString();
    for (auto v : obj["changelog"].toArray()) {
        QJsonObject e = v.toObject();
        info.changelog.append({e["type"].toString(), e["text"].toString()});
    }
    return info;
}