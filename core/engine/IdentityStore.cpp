#include "IdentityStore.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

QString IdentityStore::identityPath() {
    QString configDir = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + "/identity.json";
}

bool IdentityStore::exists() {
    return QFile::exists(identityPath());
}

BrowserIdentity IdentityStore::load() {
    if (!exists()) return generateAndSave();

    QFile f(identityPath());
    if (!f.open(QIODevice::ReadOnly)) return generateAndSave();

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isNull() || !doc.isObject()) return generateAndSave();

    BrowserIdentity id = BrowserIdentity::fromJson(doc.object());
    if (!id.isValid()) return generateAndSave();

    return id;
}

BrowserIdentity IdentityStore::regenerate() {
    QFile::remove(identityPath());
    return generateAndSave();
}

BrowserIdentity IdentityStore::generateAndSave() {
    BrowserIdentity id = IdentityGenerator::generate();
    save(id);
    return id;
}

bool IdentityStore::save(const BrowserIdentity &id) {
    QFile f(identityPath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(id.toJson()).toJson(QJsonDocument::Indented));
    return true;
}