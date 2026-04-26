#include "SessionManager.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QWebEngineCookieStore>
#include <QWebEngineSettings>
#include <QDateTime>
#include <QUrl>

// ─── Path helpers ─────────────────────────────────────────────────────────────

QString SessionManager::binaryDir() {
    return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
}

QString SessionManager::cookiesPath() {
    return binaryDir() + "/cookies.json";
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
    // Watch both files — even if they don't exist yet (we re-add after create)
    QString cp = cookiesPath();
    QString pp = profilePath();

    if (QFile::exists(cp)) m_watcher.addPath(cp);
    if (QFile::exists(pp)) m_watcher.addPath(pp);

    // Also watch the directory so we catch new file creation
    m_watcher.addPath(binaryDir());

    loadProfile();
    loadCookies();
}

// ─── File watcher ─────────────────────────────────────────────────────────────

void SessionManager::onFileChanged(const QString &path) {
    // Qt removes a path from the watcher if the file is replaced (some editors
    // do write-new + rename). Re-add it.
    if (!m_watcher.files().contains(path) && QFile::exists(path))
        m_watcher.addPath(path);

    if (path == cookiesPath() || path == binaryDir()) {
        qDebug() << "[SessionManager] Reloading cookies from" << path;
        loadCookies();
    }
    if (path == profilePath()) {
        qDebug() << "[SessionManager] Reloading profile from" << path;
        loadProfile();
    }
}

// ─── Load cookies ─────────────────────────────────────────────────────────────

void SessionManager::loadCookies() {
    QString path = cookiesPath();
    if (!QFile::exists(path)) {
        qDebug() << "[SessionManager] No cookies.json found at" << path;
        return;
    }

    QJsonArray arr = readCookiesFile();
    if (arr.isEmpty()) {
        qDebug() << "[SessionManager] cookies.json is empty or invalid";
        return;
    }

    auto *store = m_profile->cookieStore();

    // Clear existing cookies first so stale ones don't linger
    store->deleteAllCookies();

    int loaded = 0;
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        QJsonObject obj = v.toObject();

        QString name   = obj["name"].toString();
        QString value  = obj["value"].toString();
        QString domain = obj["domain"].toString();

        if (name.isEmpty() || domain.isEmpty()) {
            qWarning() << "[SessionManager] Skipping cookie with missing name/domain";
            continue;
        }

        QNetworkCookie cookie;
        cookie.setName(name.toUtf8());
        cookie.setValue(value.toUtf8());
        cookie.setDomain(domain);
        cookie.setPath(obj["path"].toString("/"));
        cookie.setSecure(obj["secure"].toBool(false));
        cookie.setHttpOnly(obj["httpOnly"].toBool(false));

        // Expires — unix timestamp, 0 or absent = session cookie
        qint64 exp = obj["expires"].toVariant().toLongLong();
        if (exp > 0)
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(exp));

        // Build the origin URL for setCookie
        // domain ".ebay.com" → "https://ebay.com"  (strip leading dot)
        QString host = domain.startsWith('.') ? domain.mid(1) : domain;
        QString scheme = cookie.isSecure() ? "https" : "http";
        QUrl origin(scheme + "://" + host);

        store->setCookie(cookie, origin);
        loaded++;
    }

    qDebug() << "[SessionManager] Loaded" << loaded << "cookies from cookies.json";
    emit cookiesLoaded(loaded);

    // Make sure file is watched (may have just been created)
    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
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

// ─── Write cookies back to file ───────────────────────────────────────────────

QJsonArray SessionManager::readCookiesFile() const {
    QFile f(cookiesPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QJsonArray();
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isArray()) return doc.array();
    return QJsonArray();
}

void SessionManager::writeCookiesFile(const QJsonArray &arr) {
    QString path = cookiesPath();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[SessionManager] Cannot write cookies.json";
        return;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.close();

    // Re-add to watcher (some editors cause removal on save)
    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);

    emit cookiesSaved();
}

// ─── saveCookieToFile — called by cookie.set command ─────────────────────────

void SessionManager::saveCookieToFile(const QNetworkCookie &cookie) {
    QJsonArray arr = readCookiesFile();

    // Find existing entry with same name+domain and replace it
    bool found = false;
    for (int i = 0; i < arr.size(); i++) {
        QJsonObject o = arr[i].toObject();
        if (o["name"].toString() == QString::fromUtf8(cookie.name()) &&
            o["domain"].toString() == cookie.domain()) {
            // Update in place
            o["value"]    = QString::fromUtf8(cookie.value());
            o["path"]     = cookie.path();
            o["secure"]   = cookie.isSecure();
            o["httpOnly"] = cookie.isHttpOnly();
            if (cookie.expirationDate().isValid())
                o["expires"] = cookie.expirationDate().toSecsSinceEpoch();
            arr[i] = o;
            found = true;
            break;
        }
    }

    if (!found) {
        // Append new entry
        QJsonObject o;
        o["name"]     = QString::fromUtf8(cookie.name());
        o["value"]    = QString::fromUtf8(cookie.value());
        o["domain"]   = cookie.domain();
        o["path"]     = cookie.path().isEmpty() ? "/" : cookie.path();
        o["secure"]   = cookie.isSecure();
        o["httpOnly"] = cookie.isHttpOnly();
        if (cookie.expirationDate().isValid())
            o["expires"] = cookie.expirationDate().toSecsSinceEpoch();
        arr.append(o);
    }

    writeCookiesFile(arr);
    qDebug() << "[SessionManager] Saved cookie:" << cookie.name() << "@" << cookie.domain();
}

// ─── removeCookieFromFile — called by cookie.delete command ──────────────────

void SessionManager::removeCookieFromFile(const QString &name, const QString &domain) {
    QJsonArray arr = readCookiesFile();
    QJsonArray updated;

    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        if (o["name"].toString() == name && o["domain"].toString() == domain)
            continue; // skip = delete
        updated.append(o);
    }

    writeCookiesFile(updated);
    qDebug() << "[SessionManager] Removed cookie:" << name << "@" << domain;
}