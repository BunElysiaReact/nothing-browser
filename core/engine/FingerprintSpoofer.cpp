#include "FingerprintSpoofer.h"
#include <QStringList>
#include <QList>

FingerprintSpoofer::FingerprintSpoofer() {
    m_fp = generate();
}

Fingerprint FingerprintSpoofer::generate() {
    Fingerprint fp;
    auto *rng = QRandomGenerator::global();

    QStringList versions = {"120","121","122","123","124"};
    fp.chromeVersion = versions[rng->bounded(versions.size())];

    fp.userAgent = QString(
        "Mozilla/5.0 (X11; Linux x86_64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/%1.0.0.0 Safari/537.36"
    ).arg(fp.chromeVersion);

    fp.sec_ch_ua = QString(
        "\"Chromium\";v=\"%1\", \"Not(A:Brand\";v=\"24\", \"Google Chrome\";v=\"%1\""
    ).arg(fp.chromeVersion);

    fp.platform = "Linux x86_64";
    fp.vendor   = "Google Inc.";

    QList<int> cores = {2, 4, 4, 4, 8, 8, 12, 16};
    fp.hardwareConcurrency = cores[rng->bounded(cores.size())];

    QList<int> rams = {2, 4, 4, 8, 8, 8, 16};
    fp.deviceMemory = rams[rng->bounded(rams.size())];

    QStringList screens = {
        "1920x1080", "1920x1080", "1920x1080",
        "1366x768",  "1440x900",  "1536x864",
        "2560x1440", "1280x720"
    };
    fp.screenRes = screens[rng->bounded(screens.size())];

    fp.timezone  = "Africa/Nairobi";
    fp.languages = "en-US,en;q=0.9";

    return fp;
}

QString FingerprintSpoofer::injectionScript() const {
    auto &fp = m_fp;
    QStringList res = fp.screenRes.split("x");
    QString w = res[0], h = res[1];

    return QString(R"(
        Object.defineProperty(navigator, 'webdriver', { get: () => undefined });
        Object.defineProperty(navigator, 'hardwareConcurrency', { get: () => %1 });
        Object.defineProperty(navigator, 'deviceMemory',        { get: () => %2 });
        Object.defineProperty(navigator, 'platform', { get: () => '%3' });
        Object.defineProperty(navigator, 'vendor',   { get: () => '%4' });
        Object.defineProperty(navigator, 'languages', { get: () => ['en-US','en'] });
        Object.defineProperty(navigator, 'plugins', {
            get: () => {
                var arr = [1,2,3,4,5];
                arr.item = i => arr[i];
                arr.namedItem = () => null;
                arr.refresh = () => {};
                return arr;
            }
        });
        Object.defineProperty(screen, 'width',       { get: () => %5 });
        Object.defineProperty(screen, 'height',      { get: () => %6 });
        Object.defineProperty(screen, 'availWidth',  { get: () => %5 });
        Object.defineProperty(screen, 'availHeight', { get: () => %6 - 40 });
        Object.defineProperty(screen, 'colorDepth',  { get: () => 24 });
        Object.defineProperty(screen, 'pixelDepth',  { get: () => 24 });
        window.chrome = { runtime: {}, loadTimes: function() {}, csi: function() {}, app: {} };
        const getParam = WebGLRenderingContext.prototype.getParameter;
        WebGLRenderingContext.prototype.getParameter = function(param) {
            if (param === 37445) return 'Intel Inc.';
            if (param === 37446) return 'Intel Iris OpenGL Engine';
            return getParam.call(this, param);
        };
        Intl.DateTimeFormat = new Proxy(Intl.DateTimeFormat, {
            construct(target, args) {
                if (!args[1]) args[1] = {};
                args[1].timeZone = 'Africa/Nairobi';
                return new target(...args);
            }
        });
    )")
    .arg(fp.hardwareConcurrency)
    .arg(fp.deviceMemory)
    .arg(fp.platform)
    .arg(fp.vendor)
    .arg(w).arg(h);
}