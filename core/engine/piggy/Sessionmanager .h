#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QWebEngineProfile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QList>
#include <QNetworkCookie>

// ─────────────────────────────────────────────────────────────────────────────
//  SessionManager
//
//  Loads cookies.json and profile.json from the same folder as the binary.
//  Both files are watched — any change on disk is applied live, no restart.
//
//  cookies.json  — standard browser cookie format (Chrome DevTools / Playwright)
//  profile.json  — UA string, web settings overrides
//
//  cookies.json format:
//  [
//    {
//      "name":     "session_id",       // required
//      "value":    "abc123",           // required
//      "domain":   ".ebay.com",        // required  (leading dot = include subdomains)
//      "path":     "/",               // optional, default "/"
//      "secure":   true,              // optional, default false
//      "httpOnly": true,              // optional, default false
//      "sameSite": "Lax",            // optional: Strict | Lax | None
//      "expires":  1735689600        // optional: unix timestamp, omit = session cookie
//    }
//  ]
//
//  profile.json format:
//  {
//    "userAgent": "Mozilla/5.0 ...",
//    "language":  "en-US",
//    "javascriptEnabled": true,
//    "imagesEnabled":     true,
//    "webglEnabled":      true,
//    "localStorageEnabled": true
//  }
// ─────────────────────────────────────────────────────────────────────────────

class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QWebEngineProfile *profile, QObject *parent = nullptr);

    // ── Initial load (call once after profile is set up) ──────────────────────
    void load();

    // ── Called by cookie.set / cookie.delete commands ─────────────────────────
    // Writes the change back to cookies.json on disk immediately.
    void saveCookieToFile(const QNetworkCookie &cookie);
    void removeCookieFromFile(const QString &name, const QString &domain);

    // ── Path helpers ──────────────────────────────────────────────────────────
    static QString binaryDir();   // folder containing the binary
    static QString cookiesPath(); // <binaryDir>/cookies.json
    static QString profilePath(); // <binaryDir>/profile.json

signals:
    void cookiesLoaded(int count);
    void profileLoaded();
    void cookiesSaved();

private slots:
    void onFileChanged(const QString &path);

private:
    void loadCookies();
    void loadProfile();
    void writeCookiesFile(const QJsonArray &arr);
    QJsonArray readCookiesFile() const;

    QWebEngineProfile    *m_profile = nullptr;
    QFileSystemWatcher    m_watcher;
};