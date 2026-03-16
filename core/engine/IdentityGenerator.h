#pragma once
#include <QString>
#include <QJsonObject>
#include <QRandomGenerator>

// ── The full generated identity ───────────────────────────────────────────────
struct BrowserIdentity {
    // Real machine values — read at runtime
    QString gpuVendorFull;
    QString gpuRenderer;
    int     cpuCores   = 4;
    int     ramGb      = 8;
    int     screenW    = 1920;
    int     screenH    = 1080;
    double  pixelRatio = 1.0;
    QString platform;
    QString os;
    QString timezone;
    QString language   = "en-US";

    // Browser
    int     chromeVersion = 124;
    QString userAgent;
    QString secChUa;

    // Noise seeds — randomised per session
    // same seed within a session = APIs are consistent with each other
    // different seed each session = cross-site tracking fails
    double  canvasSeed;
    double  audioSeed;
    double  webglSeed;
    double  fontSeed;

    QString generatedAt;
    QString identityId;

    QJsonObject toJson() const;
    static BrowserIdentity fromJson(const QJsonObject &obj);
    bool isValid() const { return !identityId.isEmpty(); }
};

class IdentityGenerator {
public:
    static BrowserIdentity generate();
private:
    static QString buildUserAgent(const QString &os, int chromeVer);
    static QString buildSecChUa(int chromeVer);
    static QString generateId();
};