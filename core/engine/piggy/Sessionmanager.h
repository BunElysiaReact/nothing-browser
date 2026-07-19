#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QWebEngineProfile>
#include <QJsonObject>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
//  SessionManager
//
//  Loads profile.json from the same folder as the binary.
//  The file is watched — any change on disk is applied live, no restart.
//
//  profile.json  — UA string, web settings overrides
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
//
//  NOTE: cookies.json / cookie persistence has been removed. Nothing Browser
//  is intentionally volatile — sessions do not survive process restart.
//  See: https://nothing-browser-docs.pages.dev/blog/piggy/somegreatnews
// ─────────────────────────────────────────────────────────────────────────────

class SessionManager : public QObject {
Q_OBJECT
public:
explicit SessionManager(QWebEngineProfile *profile, QObject *parent = nullptr);

    // ── Initial load (call once after profile is set up) ──────────────────────
void load();

    // ── Path helpers ──────────────────────────────────────────────────────────
static QString binaryDir();   // folder containing the binary
static QString profilePath(); // <binaryDir>/profile.json

signals:
void profileLoaded();

private slots:
void onFileChanged(const QString &path);

private:
void loadProfile();

    QWebEngineProfile    *m_profile = nullptr;
    QFileSystemWatcher    m_watcher;
};