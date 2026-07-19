#pragma once
#include "IdentityGenerator.h"
#include <QString>

// ── Saves and loads identity from ~/.config/nothing-browser/identity.json ────
// If no identity exists, generates one and saves it.
// Identity is permanent — same machine, same fingerprint, forever.
class IdentityStore {
public:
    // Returns the persistent identity — generates if first launch
    static BrowserIdentity load();

    // Force regenerate (user clicked "Reset Identity")
    static BrowserIdentity regenerate();

    // Get path to identity file
    static QString identityPath();

    // Check if identity file exists
    static bool exists();

private:
    static BrowserIdentity generateAndSave();
    static bool save(const BrowserIdentity &id);
};