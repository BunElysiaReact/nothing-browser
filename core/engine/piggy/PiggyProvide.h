#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>

class PiggyServer;

// ─── provide.* command handler ────────────────────────────────────────────────
// The "killer feature" — structured DOM extraction without raw evaluate().
//
//   provide.text      { selector }               → string
//   provide.textAll   { selector }               → string[]
//   provide.attr      { selector, attr }         → string
//   provide.attrAll   { selector, attr }         → string[]
//   provide.html      { selector }               → string
//   provide.table     { selector }               → { headers:[], rows:[[]] }
//   provide.list      { selector, itemSel? }     → string[]
//   provide.links     { selector? }              → { text, href }[]
//   provide.images    { selector? }              → { alt, src }[]
//   provide.form      { selector }               → { [name]: value }
//   provide.page      {}                         → { title, url, html, text }
//   provide.div       { selector }               → { tag, id, cls, text, html, children:[] }
//   provide.meta      {}                         → { [name]: content }
//   provide.select    { selector }               → { value, options:[{text,value,selected}] }
//   provide.json      { selector? }              → parsed JSON from element text or script[type=application/json]
// ─────────────────────────────────────────────────────────────────────────────

bool piggy_handleProvide(PiggyServer *srv, const QString &c,
                         const QJsonObject &payload,
                         QLocalSocket *client, const QString &id,
                         const QString &tabId);