#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>

class PiggyServer;

// ─── find.* command handler ───────────────────────────────────────────────────
// Implements:
//   find.byText        { selector?, text, exact? }
//   find.byAttr        { attr, value, selector? }
//   find.byTag         { tag }
//   find.byPlaceholder { text }
//   find.byRole        { role, name? }
//   find.css           { selector }          (returns full element list)
//   find.first         { selector }
//   find.all           { selector }
//   find.closest       { selector, ancestor }
//   find.parent        { selector }
//   find.children      { selector }
//   find.filter        { selector, attr, value }
//   find.count         { selector }
//   find.exists        { selector }
//   find.visible       { selector }
//   find.enabled       { selector }
//   find.checked       { selector }
//
// Every find.* result is a JSON array of element descriptors:
//   { tag, id, cls, text, html, href, src, value, attrs: {} }
// ─────────────────────────────────────────────────────────────────────────────

bool piggy_handleFind(PiggyServer *srv, const QString &c,
                      const QJsonObject &payload,
                      QLocalSocket *client, const QString &id,
                      const QString &tabId);