#pragma once
#include "IdentityStore.h"
#include <QString>

class FingerprintSpoofer {
public:
    static FingerprintSpoofer &instance() {
        static FingerprintSpoofer s;
        return s;
    }

    const BrowserIdentity &identity() const { return m_id; }

    // Full JS injection script — injected at DocumentCreation
    // covers every fingerprint vector
    QString injectionScript() const;

    // Force load a specific identity (for testing / headless mode)
    void loadIdentity(const BrowserIdentity &id) { m_id = id; }

    // Regenerate identity (user clicked reset)
    void resetIdentity() { m_id = IdentityStore::regenerate(); }

private:
    FingerprintSpoofer() { m_id = IdentityStore::load(); }
    BrowserIdentity m_id;
};