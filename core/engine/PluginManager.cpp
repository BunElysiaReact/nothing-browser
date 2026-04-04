#include "PluginManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QNetworkRequest>

PluginManager::PluginManager() {
    m_nam = new QNetworkAccessManager(this);
    scanInstalled();
    loadState();
}

QString PluginManager::pluginsDir() const {
    QString d = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                + "/plugins";
    QDir().mkpath(d);
    return d;
}

void PluginManager::scanInstalled() {
    m_installed.clear();
    QDir dir(pluginsDir());
    for (auto &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString manifestPath = dir.filePath(sub + "/manifest.json");
        if (!QFile::exists(manifestPath)) continue;
        PluginManifest pm = parseManifest(manifestPath);
        pm.installed   = true;
        pm.installPath = dir.filePath(sub);
        m_installed.append(pm);
    }
}

PluginManifest PluginManager::parseManifest(const QString &path) {
    PluginManifest pm;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return pm;
    QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    pm.id              = o["id"].toString();
    pm.name            = o["name"].toString();
    pm.version         = o["version"].toString();
    pm.description     = o["description"].toString();
    pm.author          = o["author"].toString();
    pm.howToUse        = o["how_to_use"].toString();
    pm.requiresRestart = o["requires_restart"].toBool();
    pm.enabled         = o["enabled"].toBool(true);
    for (auto v : o["permissions"].toArray())
        pm.permissions << v.toString();
    return pm;
}

void PluginManager::injectAll(QWebEngineProfile *profile) {
    for (auto &pm : m_installed) {
        if (!pm.enabled) continue;
        QString jsPath = pm.installPath + "/content.js";
        if (!QFile::exists(jsPath)) continue;
        QFile f(jsPath);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QWebEngineScript s;
        s.setName("plugin_" + pm.id);
        s.setSourceCode(QString::fromUtf8(f.readAll()));
        s.setInjectionPoint(QWebEngineScript::DocumentCreation);
        s.setWorldId(QWebEngineScript::MainWorld);
        s.setRunsOnSubFrames(true);
        profile->scripts()->insert(s);
    }
}

bool PluginManager::installPlugin(const QString &folderPath) {
    QString manifestPath = folderPath + "/manifest.json";
    if (!QFile::exists(manifestPath)) return false;
    PluginManifest pm = parseManifest(manifestPath);
    if (pm.id.isEmpty()) return false;

    QString dest = pluginsDir() + "/" + pm.id;
    QDir().mkpath(dest);

    // Copy all files
    QDir src(folderPath);
    for (auto &f : src.entryList(QDir::Files)) {
        QString destFile = dest + "/" + f;
        QFile::remove(destFile);
        QFile::copy(src.filePath(f), destFile);
    }
    scanInstalled();
    loadState();
    emit pluginListChanged();
    return true;
}

bool PluginManager::uninstallPlugin(const QString &id) {
    QString path = pluginsDir() + "/" + id;
    QDir dir(path);
    if (!dir.exists()) return false;
    dir.removeRecursively();
    scanInstalled();
    emit pluginListChanged();
    return true;
}

void PluginManager::setEnabled(const QString &id, bool enabled) {
    for (auto &pm : m_installed) {
        if (pm.id != id) continue;
        pm.enabled = enabled;
        // Patch manifest on disk
        QString path = pm.installPath + "/manifest.json";
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        o["enabled"] = enabled;
        if (!f.open(QIODevice::WriteOnly)) return;
        f.write(QJsonDocument(o).toJson());
        break;
    }
    emit pluginListChanged();
}

void PluginManager::fetchCommunityList() {
    QNetworkRequest req{QUrl(REPO_API)};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "NothingBrowser");
    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        m_community.clear();

        // Each entry is a folder — fetch manifest.json for each
        int *pending = new int(arr.size());
        if (*pending == 0) { delete pending; emit communityListReady(m_community); return; }

        for (auto v : arr) {
            QJsonObject entry = v.toObject();
            if (entry["type"].toString() != "dir") { if (--(*pending) == 0) { delete pending; emit communityListReady(m_community); } continue; }
            QString pluginId = entry["name"].toString();
            QString rawUrl   = QString("%1/%2/manifest.json").arg(REPO_RAW).arg(pluginId);
            QNetworkRequest mreq{QUrl(rawUrl)};
            mreq.setHeader(QNetworkRequest::UserAgentHeader, "NothingBrowser");
            auto *mr = m_nam->get(mreq);
            connect(mr, &QNetworkReply::finished, this, [this, mr, pending]() {
                mr->deleteLater();
                if (mr->error() == QNetworkReply::NoError) {
                    QJsonObject o = QJsonDocument::fromJson(mr->readAll()).object();
                    PluginManifest pm;
                    pm.id              = o["id"].toString();
                    pm.name            = o["name"].toString();
                    pm.version         = o["version"].toString();
                    pm.description     = o["description"].toString();
                    pm.author          = o["author"].toString();
                    pm.howToUse        = o["how_to_use"].toString();
                    pm.requiresRestart = o["requires_restart"].toBool();
                    pm.installed       = false;
                    for (auto pv : o["permissions"].toArray()) pm.permissions << pv.toString();
                    // Mark if already installed
                    for (auto &ip : m_installed) if (ip.id == pm.id) { pm.installed = true; pm.enabled = ip.enabled; break; }
                    m_community.append(pm);
                }
                if (--(*pending) == 0) { delete pending; emit communityListReady(m_community); }
            });
        }
    });
}

void PluginManager::installFromRepo(const QString &pluginId) {
    // Files to fetch — manifest + content.js + optional background.js + icon.png
    QStringList files = {"manifest.json", "content.js", "background.js", "icon.png"};
    QString dest = pluginsDir() + "/" + pluginId;
    QDir().mkpath(dest);

    int *pending = new int(files.size());
    bool *anyFailed = new bool(false);

    for (auto &fname : files) {
        QString url = QString("%1/%2/%3").arg(REPO_RAW).arg(pluginId).arg(fname);
        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::UserAgentHeader, "NothingBrowser");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, dest, fname, pluginId, pending, anyFailed]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QFile f(dest + "/" + fname);
                if (f.open(QIODevice::WriteOnly))
                    f.write(reply->readAll());
            } else if (fname == "manifest.json" || fname == "content.js") {
                *anyFailed = true;
            }
            if (--(*pending) == 0) {
                bool ok = !*anyFailed;
                delete pending; delete anyFailed;
                if (ok) { scanInstalled(); loadState(); emit pluginListChanged(); }
                emit installComplete(pluginId, ok, ok ? "" : "Failed to fetch required files");
            }
        });
    }
}

void PluginManager::saveState() {}
void PluginManager::loadState() {}