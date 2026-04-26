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
//  All data files live in the CURRENT WORKING DIRECTORY — i.e. the folder the
//  user ran their script from, e.g. ~/projects/my-scraper/.
//
//  Files auto-created on first use:
//    cookies.json  — persistent cookies (preloaded on every start, hot-reloaded on change)
//    profile.json  — UA, language, web settings
//    ws.json       — WebSocket frames (opt-in via saveWs flag)
//    pings.json    — health/ping log (opt-in via savePings flag)
//
//  cookies.json format:
//  [
//    {
//      "name":     "session_id",    // required
//      "value":    "abc123",        // required
//      "domain":   ".ebay.com",     // required
//      "path":     "/",             // optional, default "/"
//      "secure":   true,            // optional, default false
//      "httpOnly": true,            // optional, default false
//      "expires":  1735689600       // optional: unix timestamp
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

    // ── Opt-in flags for optional capture files ───────────────────────────────
    void setSaveWs(bool on)    { m_saveWs = on; }
    void setSavePings(bool on) { m_savePings = on; }
    bool saveWs()    const { return m_saveWs; }
    bool savePings() const { return m_savePings; }

    // ── Called by cookie.set / cookie.delete commands ─────────────────────────
    void saveCookieToFile(const QNetworkCookie &cookie);
    void removeCookieFromFile(const QString &name, const QString &domain);

    // ── Called by capture to append to optional logs ──────────────────────────
    void appendWsFrame(const QJsonObject &frame);   // only writes if m_saveWs
    void appendPing(const QJsonObject &ping);       // only writes if m_savePings

    // ── Path helpers — all rooted at QDir::currentPath() ─────────────────────
    static QString workDir();     // QDir::currentPath()
    static QString cookiesPath(); // <workDir>/cookies.json
    static QString profilePath(); // <workDir>/profile.json
    static QString wsPath();      // <workDir>/ws.json
    static QString pingsPath();   // <workDir>/pings.json

signals:
    void cookiesLoaded(int count);
    void profileLoaded();
    void cookiesSaved();

private slots:
    void onFileChanged(const QString &path);

private:
    void loadCookies();
    void loadProfile();
    void ensureWorkDir();
    void writeCookiesFile(const QJsonArray &arr);
    QJsonArray readCookiesFile() const;
    void appendToJsonArrayFile(const QString &path, const QJsonObject &entry);

    QWebEngineProfile    *m_profile   = nullptr;
    QFileSystemWatcher    m_watcher;
    bool                  m_saveWs    = false;
    bool                  m_savePings = false;
};