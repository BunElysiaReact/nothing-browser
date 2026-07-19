#include "IdentityGenerator.h"
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>
#include <QFile>
#include <QRegularExpression>
#include <QSysInfo>
#include <QScreen>
#include <QGuiApplication>
#include <QTimeZone>
#include <thread>

BrowserIdentity IdentityGenerator::generate() {
    auto *rng = QRandomGenerator::global();

    BrowserIdentity id;

    // ── Real CPU cores ────────────────────────────────────────────────────────
    id.cpuCores = (int)std::thread::hardware_concurrency();
    if (id.cpuCores < 1) id.cpuCores = 4;

    // ── Real RAM ──────────────────────────────────────────────────────────────
    id.ramGb = 8;
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly)) {
        QString line = meminfo.readLine();
        while (!line.isEmpty()) {
            if (line.startsWith("MemTotal:")) {
                long long kb = line.split(QRegularExpression("\\s+"))[1].toLongLong();
                long long gb = kb / 1024 / 1024;
                if      (gb <= 1)  id.ramGb = 1;
                else if (gb <= 2)  id.ramGb = 2;
                else if (gb <= 4)  id.ramGb = 4;
                else if (gb <= 8)  id.ramGb = 8;
                else if (gb <= 16) id.ramGb = 16;
                else               id.ramGb = 32;
                break;
            }
            line = meminfo.readLine();
        }
    }

    // ── Real screen resolution ────────────────────────────────────────────────
    if (QGuiApplication::instance()) {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            id.screenW    = screen->size().width();
            id.screenH    = screen->size().height();
            id.pixelRatio = screen->devicePixelRatio();
        }
    } else {
        id.screenW    = 1920;
        id.screenH    = 1080;
        id.pixelRatio = 1.0;
    }

    // ── Real platform ─────────────────────────────────────────────────────────
#if defined(Q_OS_WIN)
    id.platform = "Win32";
    id.os       = "Windows NT 10.0; Win64; x64";
#elif defined(Q_OS_MAC)
    id.platform = "MacIntel";
    id.os       = "Macintosh; Intel Mac OS X 10_15_7";
#else
    id.platform = "Linux x86_64";
    id.os       = "X11; Linux x86_64";
#endif

    // ── Real timezone ─────────────────────────────────────────────────────────
    id.timezone = QString::fromLatin1(QTimeZone::systemTimeZoneId());
    if (id.timezone.isEmpty()) id.timezone = "UTC";

    // ── Language ──────────────────────────────────────────────────────────────
    id.language = "en-US";

    // ── GPU ───────────────────────────────────────────────────────────────────
    id.gpuVendorFull = "Google Inc. (Intel)";
    id.gpuRenderer   = "ANGLE (Intel, Mesa Intel(R) HD Graphics 3000 (SNB GT2), OpenGL 4.6)";

    QFile gpuFile("/sys/class/drm/card0/device/vendor");
    if (gpuFile.open(QIODevice::ReadOnly)) {
        QString vendorId = QString(gpuFile.readAll()).trimmed();
        if      (vendorId == "0x10de") id.gpuVendorFull = "Google Inc. (NVIDIA)";
        else if (vendorId == "0x1002") id.gpuVendorFull = "Google Inc. (AMD)";
        else                           id.gpuVendorFull = "Google Inc. (Intel)";
    }

    // ── Chrome version ────────────────────────────────────────────────────────
    id.chromeVersion = 124;

    // ── UA + sec-ch-ua ────────────────────────────────────────────────────────
    id.userAgent = buildUserAgent(id.os, id.chromeVersion);
    id.secChUa   = buildSecChUa(id.chromeVersion);

    // ── Noise seeds ───────────────────────────────────────────────────────────
    id.canvasSeed = rng->generateDouble();
    id.audioSeed  = rng->generateDouble();
    id.webglSeed  = rng->generateDouble();
    id.fontSeed   = rng->generateDouble();

    id.generatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    id.identityId  = generateId();

    return id;
}

QString IdentityGenerator::buildUserAgent(const QString &os, int cv) {
    return QString(
        "Mozilla/5.0 (%1) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/%2.0.0.0 Safari/537.36"
    ).arg(os).arg(cv);
}

// FIX 1: Chrome 110+ changed brand string format and order
// Old: "Chromium";v="X", "Not(A:Brand";v="24", "Google Chrome";v="X"
// New: "Google Chrome";v="X", "Chromium";v="X", "Not-A.Brand";v="99"
QString IdentityGenerator::buildSecChUa(int cv) {
    return QString(
        "\"Google Chrome\";v=\"%1\", "
        "\"Chromium\";v=\"%1\", "
        "\"Not-A.Brand\";v=\"99\""
    ).arg(cv);
}

QString IdentityGenerator::generateId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QJsonObject BrowserIdentity::toJson() const {
    QJsonObject o;
    o["identity_id"]    = identityId;
    o["generated_at"]   = generatedAt;
    o["gpu_vendor"]     = gpuVendorFull;
    o["gpu_renderer"]   = gpuRenderer;
    o["cpu_cores"]      = cpuCores;
    o["ram_gb"]         = ramGb;
    o["screen_w"]       = screenW;
    o["screen_h"]       = screenH;
    o["pixel_ratio"]    = pixelRatio;
    o["platform"]       = platform;
    o["os"]             = os;
    o["timezone"]       = timezone;
    o["language"]       = language;
    o["chrome_version"] = chromeVersion;
    o["user_agent"]     = userAgent;
    o["sec_ch_ua"]      = secChUa;
    o["canvas_seed"]    = canvasSeed;
    o["audio_seed"]     = audioSeed;
    o["webgl_seed"]     = webglSeed;
    o["font_seed"]      = fontSeed;
    return o;
}

BrowserIdentity BrowserIdentity::fromJson(const QJsonObject &o) {
    BrowserIdentity id;
    id.identityId    = o["identity_id"].toString();
    id.generatedAt   = o["generated_at"].toString();
    id.gpuVendorFull = o["gpu_vendor"].toString();
    id.gpuRenderer   = o["gpu_renderer"].toString();
    id.cpuCores      = o["cpu_cores"].toInt();
    id.ramGb         = o["ram_gb"].toInt();
    id.screenW       = o["screen_w"].toInt();
    id.screenH       = o["screen_h"].toInt();
    id.pixelRatio    = o["pixel_ratio"].toDouble();
    id.platform      = o["platform"].toString();
    id.os            = o["os"].toString();
    id.timezone      = o["timezone"].toString();
    id.language      = o["language"].toString();
    id.chromeVersion = o["chrome_version"].toInt();
    id.userAgent     = o["user_agent"].toString();
    id.secChUa       = o["sec_ch_ua"].toString();
    id.canvasSeed    = o["canvas_seed"].toDouble();
    id.audioSeed     = o["audio_seed"].toDouble();
    id.webglSeed     = o["webgl_seed"].toDouble();
    id.fontSeed      = o["font_seed"].toDouble();
    return id;
}