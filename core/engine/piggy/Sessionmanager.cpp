#include "Sessionmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QWebEngineCookieStore>
#include <QWebEngineSettings>
#include <QDateTime>
#include <QUrl>

// ─── Path helpers ─────────────────────────────────────────────────────────────

QString SessionManager::workDir() {
    // Always the directory the user ran their script from.
    // e.g. ~/projects/my-scraper/
    return QDir::currentPath();
}

QString SessionManager::cookiesPath() { return workDir() + "/cookies.json"; }
QString SessionManager::profilePath() { return workDir() + "/profile.json"; }
QString SessionManager::wsPath()      { return workDir() + "/ws.json";      }
QString SessionManager::pingsPath()   { return workDir() + "/pings.json";   }

// ─── Constructor ──────────────────────────────────────────────────────────────

SessionManager::SessionManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &SessionManager::onFileChanged);
}

// ─── ensureWorkDir ────────────────────────────────────────────────────────────

void SessionManager::ensureWorkDir() {
    QDir().mkpath(workDir());
}

// ─── Initial load ─────────────────────────────────────────────────────────────

void SessionManager::load() {
    ensureWorkDir();

    QString cp = cookiesPath();
    QString pp = profilePath();

    // Auto-create empty cookies.json if missing so users see the format.
    if (!QFile::exists(cp)) {
        QFile f(cp);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write("[\n]\n");
        qDebug() << "[SessionManager] Created empty cookies.json at" << cp;
    }

    // Auto-create default profile.json if missing.
    if (!QFile::exists(pp)) {
        QJsonObject defaults;
        defaults["userAgent"]           = m_profile->httpUserAgent();
        defaults["language"]            = "en-US";
        defaults["javascriptEnabled"]   = true;
        defaults["imagesEnabled"]       = true;
        defaults["webglEnabled"]        = true;
        defaults["localStorageEnabled"] = true;
        QFile f(pp);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(QJsonDocument(defaults).toJson(QJsonDocument::Indented));
        qDebug() << "[SessionManager] Created default profile.json at" << pp;
    }

    if (QFile::exists(cp)) m_watcher.addPath(cp);
    if (QFile::exists(pp)) m_watcher.addPath(pp);
    m_watcher.addPath(workDir()); // catch new file creation

    loadProfile();
    loadCookies();
}

// ─── File watcher ─────────────────────────────────────────────────────────────

void SessionManager::onFileChanged(const QString &path) {
    // Re-add if editor replaced the file (write-new + rename)
    if (!m_watcher.files().contains(path) && QFile::exists(path))
        m_watcher.addPath(path);

    if (path == cookiesPath() || path == workDir()) {
        qDebug() << "[SessionManager] Hot-reloading cookies from" << path;
        loadCookies();
    }
    if (path == profilePath()) {
        qDebug() << "[SessionManager] Hot-reloading profile from" << path;
        loadProfile();
    }
}

// ─── Load cookies ─────────────────────────────────────────────────────────────

void SessionManager::loadCookies() {
    QJsonArray arr = readCookiesFile();
    if (arr.isEmpty()) return;

    auto *store = m_profile->cookieStore();
    store->deleteAllCookies();

    int loaded = 0;
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        QJsonObject obj = v.toObject();

        QString name   = obj["name"].toString();
        QString domain = obj["domain"].toString();
        if (name.isEmpty() || domain.isEmpty()) continue;

        QNetworkCookie cookie;
        cookie.setName(name.toUtf8());
        cookie.setValue(obj["value"].toString().toUtf8());
        cookie.setDomain(domain);
        cookie.setPath(obj["path"].toString("/"));
        cookie.setSecure(obj["secure"].toBool(false));
        cookie.setHttpOnly(obj["httpOnly"].toBool(false));

        qint64 exp = obj["expires"].toVariant().toLongLong();
        if (exp > 0)
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(exp));

        QString host   = domain.startsWith('.') ? domain.mid(1) : domain;
        QString scheme = cookie.isSecure() ? "https" : "http";
        store->setCookie(cookie, QUrl(scheme + "://" + host));
        loaded++;
    }

    qDebug() << "[SessionManager] Loaded" << loaded << "cookies from" << cookiesPath();
    emit cookiesLoaded(loaded);

    if (!m_watcher.files().contains(cookiesPath()))
        m_watcher.addPath(cookiesPath());
}

// ─── Load profile ─────────────────────────────────────────────────────────────

void SessionManager::loadProfile() {
    QFile f(profilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    auto *s = m_profile->settings();

    if (obj.contains("userAgent")) {
        QString ua = obj["userAgent"].toString();
        if (!ua.isEmpty()) m_profile->setHttpUserAgent(ua);
    }
    if (obj.contains("language"))
        m_profile->setHttpAcceptLanguage(obj["language"].toString());
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

    qDebug() << "[SessionManager] Profile loaded from" << profilePath();
    emit profileLoaded();

    if (!m_watcher.files().contains(profilePath()))
        m_watcher.addPath(profilePath());
}

// ─── Cookie file helpers ──────────────────────────────────────────────────────

QJsonArray SessionManager::readCookiesFile() const {
    QFile f(cookiesPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QJsonArray();
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isArray() ? doc.array() : QJsonArray();
}

void SessionManager::writeCookiesFile(const QJsonArray &arr) {
    QString path = cookiesPath();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[SessionManager] Cannot write" << path;
        return;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.close();
    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
    emit cookiesSaved();
}

void SessionManager::saveCookieToFile(const QNetworkCookie &cookie) {
    QJsonArray arr = readCookiesFile();
    bool found = false;
    for (int i = 0; i < arr.size(); i++) {
        QJsonObject o = arr[i].toObject();
        if (o["name"].toString()   == QString::fromUtf8(cookie.name()) &&
            o["domain"].toString() == cookie.domain()) {
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

void SessionManager::removeCookieFromFile(const QString &name, const QString &domain) {
    QJsonArray arr = readCookiesFile(), updated;
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        if (o["name"].toString() == name && o["domain"].toString() == domain) continue;
        updated.append(o);
    }
    writeCookiesFile(updated);
    qDebug() << "[SessionManager] Removed cookie:" << name << "@" << domain;
}

// ─── Append helpers for optional capture files ────────────────────────────────

void SessionManager::appendToJsonArrayFile(const QString &path, const QJsonObject &entry) {
    // Read existing array (or start fresh), append entry, write back.
    QJsonArray arr;
    QFile rf(path);
    if (rf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(rf.readAll());
        if (doc.isArray()) arr = doc.array();
        rf.close();
    }
    arr.append(entry);
    QFile wf(path);
    if (wf.open(QIODevice::WriteOnly | QIODevice::Text))
        wf.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void SessionManager::appendWsFrame(const QJsonObject &frame) {
    if (!m_saveWs) return;
    appendToJsonArrayFile(wsPath(), frame);
}

void SessionManager::appendPing(const QJsonObject &ping) {
    if (!m_savePings) return;
    appendToJsonArrayFile(pingsPath(), ping);
}