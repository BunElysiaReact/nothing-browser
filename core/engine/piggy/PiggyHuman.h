#pragma once
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>

class PiggyServer;

// ─── human.* — human-like behavior profiles ───────────────────────────────────
//
//   human.set     { typingSpeed, clickDelay, scrollSpeed, mouseWiggle? }
//     typingSpeed:  "slow" | "normal" | "fast" (chars per sec: 3 | 8 | 18)
//     clickDelay:   "cautious" | "normal" | "fast" (ms before click: 600 | 200 | 50)
//     scrollSpeed:  "slow" | "normal" | "fast"
//     mouseWiggle:  bool — move mouse before clicks (default true)
//
//   human.type    { selector, text, clear?, speed? }
//     Like type but sends character-by-character with realistic delays.
//     clear: true — clears field first (Ctrl+A + Delete)
//
//   human.click   { selector, force? }
//     force: true — scrolls into view + dispatches all mouse events manually
//     even if element is covered. Falls back to el.click() if accessible.
//
//   human.get     {}
//     Returns current profile settings.
//
// ─────────────────────────────────────────────────────────────────────────────

struct HumanProfile {
    QString typingSpeed = "normal";   // slow|normal|fast
    QString clickDelay  = "normal";   // cautious|normal|fast
    QString scrollSpeed = "normal";   // slow|normal|fast
    bool    mouseWiggle = true;

    int typingDelayMs() const {
        if (typingSpeed == "slow")   return 300;
        if (typingSpeed == "fast")   return 40;
        return 120;  // normal
    }
    int clickDelayMs() const {
        if (clickDelay == "cautious") return 600;
        if (clickDelay == "fast")     return 50;
        return 200;  // normal
    }
};

bool piggy_handleHuman(PiggyServer *srv, const QString &c,
                       const QJsonObject &payload,
                       QLocalSocket *client, const QString &id,
                       const QString &tabId);

// Global profile accessor
HumanProfile &piggy_humanProfile();