#include "Sessionmanager.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebEngineSettings>

// ─── Path helpers ─────────────────────────────────────────────────────────────

QString SessionManager::binaryDir() {
    return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
}

QString SessionManager::profilePath() {
    return binaryDir() + "/profile.json";
}

// ─── Constructor ──────────────────────────────────────────────────────────────

SessionManager::SessionManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &SessionManager::onFileChanged);
}

// ─── Initial load ─────────────────────────────────────────────────────────────

void SessionManager::load() {
    QString pp = profilePath();

    if (QFile::exists(pp)) m_watcher.addPath(pp);

    // Also watch the directory so we catch new file creation
    m_watcher.addPath(binaryDir());

    loadProfile();
}

// ─── File watcher ─────────────────────────────────────────────────────────────

void SessionManager::onFileChanged(const QString &path) {
    // Qt removes a path from the watcher if the file is replaced (some editors
    // do write-new + rename). Re-add it.
    if (!m_watcher.files().contains(path) && QFile::exists(path))
        m_watcher.addPath(path);

    if (path == profilePath()) {
        qDebug() << "[SessionManager] Reloading profile from" << path;
        loadProfile();
    }
}

// ─── Load profile ─────────────────────────────────────────────────────────────

void SessionManager::loadProfile() {
    QString path = profilePath();
    if (!QFile::exists(path)) {
        qDebug() << "[SessionManager] No profile.json found at" << path;
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[SessionManager] Cannot open profile.json";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        qWarning() << "[SessionManager] profile.json is not a valid JSON object";
        return;
    }

    QJsonObject obj = doc.object();

    // User agent
    if (obj.contains("userAgent")) {
        QString ua = obj["userAgent"].toString();
        if (!ua.isEmpty()) {
            m_profile->setHttpUserAgent(ua);
            qDebug() << "[SessionManager] UA set to:" << ua;
        }
    }

    // HTTP accept language
    if (obj.contains("language")) {
        m_profile->setHttpAcceptLanguage(obj["language"].toString());
    }

    // Web settings
    auto *s = m_profile->settings();

    if (obj.contains("javascriptEnabled"))
        s->setAttribute(QWebEngineSettings::JavascriptEnabled,
                        obj["javascriptEnabled"].toBool(true));

    if (obj.contains("imagesEnabled"))
        s->setAttribute(QWebEngineSettings::AutoLoadImages,
                        obj["imagesEnabled"].toBool(true));

    if (obj.contains("webglEnabled"))
        s->setAttribute(QWebEngineSettings::WebGLEnabled,
                        obj["webglEnabled"].toBool(true));

    if (obj.contains("localStorageEnabled"))
        s->setAttribute(QWebEngineSettings::LocalStorageEnabled,
                        obj["localStorageEnabled"].toBool(true));

    if (obj.contains("doNotTrack")) {
        // Qt doesn't expose DNT directly but we can set a custom header via interceptor
        // Store the value for the interceptor to use
        qDebug() << "[SessionManager] doNotTrack:" << obj["doNotTrack"].toBool();
    }

    qDebug() << "[SessionManager] Profile loaded from profile.json";
    emit profileLoaded();

    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
}