#pragma once
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>

class PiggyServer;

// ─── iframe.* command handler ─────────────────────────────────────────────────
//
//   iframe.list     { tabId }                   → [{index, src, id, name}]
//   iframe.evaluate { tabId, index|src, js }    → any    (run JS inside iframe)
//   iframe.click    { tabId, index|src, sel }   → bool
//   iframe.type     { tabId, index|src, sel, text } → bool
//   iframe.text     { tabId, index|src, sel }   → string
//   iframe.html     { tabId, index|src }        → string  (full iframe HTML)
//   iframe.waitSel  { tabId, index|src, sel, timeout? } → bool
//
// Note: QWebEngine exposes all frames via page().mainFrame() / childFrames().
// We target iframes via their index in querySelectorAll('iframe') or src match.
//
// Implementation uses runJavaScript with a world-scoped frame reference built
// by walking window.frames[n].
// ─────────────────────────────────────────────────────────────────────────────

bool piggy_handleIframe(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id,
                        const QString &tabId);