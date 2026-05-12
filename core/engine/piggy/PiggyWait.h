#pragma once
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>

class PiggyServer;

// ─── Extended wait + evaluate commands ───────────────────────────────────────
//
//   wait.function      { js, timeout? }
//     Polls every 100ms until the JS expression returns truthy.
//     Timeout defaults to 10000ms.
//
//   wait.selector      { selector, state?, timeout? }
//     state: "visible" | "hidden" | "attached" | "detached"
//     Default state: "attached" (matches existing behavior).
//     "hidden"   — waits for element to disappear or become display:none
//     "visible"  — waits for element to exist AND be visible
//     "detached" — waits for element to be removed from DOM
//
//   evaluate           { js, timeout? }
//     Runs JS with an optional wall-clock timeout.
//     On timeout responds { ok:false, error:"timeout" }.
//
//   fetch.textAll      { selector }            → string[]  (all matching)
//   fetch.attr         { selector, attr }      → string
//   fetch.attrAll      { selector, attr }      → string[]
//
// ─────────────────────────────────────────────────────────────────────────────

bool piggy_handleWait(PiggyServer *srv, const QString &c,
                      const QJsonObject &payload,
                      QLocalSocket *client, const QString &id,
                      const QString &tabId);