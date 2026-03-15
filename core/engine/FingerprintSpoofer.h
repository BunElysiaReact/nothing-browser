#pragma once
#include <QString>
#include <QRandomGenerator>

struct Fingerprint {
    QString userAgent;
    QString platform;
    QString vendor;
    int     hardwareConcurrency;
    int     deviceMemory;
    QString screenRes;
    QString timezone;
    QString languages;
    QString sec_ch_ua;
    QString chromeVersion;
};

class FingerprintSpoofer {
public:
    static FingerprintSpoofer& instance() {
        static FingerprintSpoofer s;
        return s;
    }

    const Fingerprint& get() const { return m_fp; }

    // JS to inject into every page
    QString injectionScript() const;

private:
    FingerprintSpoofer();
    Fingerprint m_fp;
    Fingerprint generate();
};