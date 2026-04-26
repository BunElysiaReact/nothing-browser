#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QLocalSocket>

class PiggyServer;

// ─── Wire ProxyManager signals → client events ────────────────────────────────
// Call once in PiggyServer constructor.
void piggy_wireProxyEvents(PiggyServer *srv);

// ─── Command dispatcher ───────────────────────────────────────────────────────
// Returns true if command was handled.
bool piggy_handleProxy(PiggyServer *srv, const QString &c,
                        const QJsonObject &payload,
                        QLocalSocket *client, const QString &id);