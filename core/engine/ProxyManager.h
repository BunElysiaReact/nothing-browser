#pragma once
#include <QObject>
#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QVector>
#include <QMutex>
#include <QElapsedTimer>
#include <QPointer>

// ── A single proxy entry ─────────────────────────────────────────────────────
struct ProxyEntry {
    enum Type   { SOCKS5, HTTP, HTTPS };
    enum Health { Unchecked, Checking, Alive, Dead };

    Type    type   = SOCKS5;
    QString host;
    quint16 port   = 1080;
    QString user;
    QString pass;

    // ── Health check results ──────────────────────────────────────────────────
    Health  health  = Unchecked;
    int     latency = -1;   // ms, -1 = not measured
    QString country;        // optional, from ip-api if enabled

    bool isValid()  const { return !host.isEmpty() && port > 0; }
    bool isUsable() const { return health != Dead; }  // Unchecked/Checking/Alive all usable

    QString toString() const;
    static ProxyEntry fromString(const QString &line);
    QNetworkProxy toQProxy() const;
};

// ── Rotation modes ───────────────────────────────────────────────────────────
enum class ProxyRotation {
    None,        // use current proxy forever
    PerRequest,  // rotate on every main-frame navigation
    Timed        // rotate every N seconds
};

// ─────────────────────────────────────────────────────────────────────────────
class ProxyManager : public QObject {
    Q_OBJECT
public:
    static ProxyManager &instance();

    // ── Load / fetch ─────────────────────────────────────────────────────────
    bool loadFromFile(const QString &path);
    void fetchFromUrl(const QString &url);
    bool loadOvpnFile(const QString &path);

    // ── Health checker ───────────────────────────────────────────────────────
    // Checks every proxy by opening a TCP connection to host:port.
    // Fast (parallel, 5s timeout per proxy). Marks Dead/Alive.
    // After checkAll() completes, dead proxies are skipped during rotation.
    void checkAll();                          // start full sweep
    void checkOne(int index);                 // check single entry
    void stopChecking();                      // abort in-flight checks
    bool isChecking() const { return m_checksPending > 0; }

    // Auto-check: run checkAll() after every load/fetch
    void setAutoCheck(bool on) { m_autoCheck = on; }
    bool autoCheck() const     { return m_autoCheck; }

    // Skip dead proxies during rotation (default: true)
    void setSkipDead(bool on) { m_skipDead = on; }
    bool skipDead()    const  { return m_skipDead; }

    // ── Rotation ─────────────────────────────────────────────────────────────
    void setRotation(ProxyRotation mode, int intervalSecs = 60);
    ProxyRotation rotation() const { return m_rotation; }

    // ── Profile binding ──────────────────────────────────────────────────────
    void applyToProfile(class QWebEngineProfile *profile);

    // ── Control ──────────────────────────────────────────────────────────────
    void next();
    void disable();
    void enableCurrent();

    // ── Queries ──────────────────────────────────────────────────────────────
    ProxyEntry  current()      const;
    int         count()        const { return m_proxies.size(); }
    int         aliveCount()   const;
    int         deadCount()    const;
    int         currentIndex() const { return m_index; }
    bool        isActive()     const { return m_active; }
    QString     ovpnRemote()   const { return m_ovpnRemote; }

    // Read-only access to full list (for UI table)
    QVector<ProxyEntry> proxies() const {
        QMutexLocker l(const_cast<QMutex*>(&m_mutex));
        return m_proxies;
    }

signals:
    void proxyChanged(const ProxyEntry &entry);
    void proxyListLoaded(int count);
    void fetchFailed(const QString &error);
    void ovpnLoaded(const QString &remote, int port);

    // Health checker signals
    void checkStarted(int total);
    void checkProgress(int index, ProxyEntry::Health result, int latencyMs);
    void checkFinished(int alive, int dead);

private:
    explicit ProxyManager(QObject *parent = nullptr);
    void rotate();
    void applyEntry(const ProxyEntry &e);
    void doCheckOne(int index);

    QVector<ProxyEntry>   m_proxies;
    int                   m_index        = 0;
    bool                  m_active       = false;
    bool                  m_autoCheck    = false;
    bool                  m_skipDead     = true;
    ProxyRotation         m_rotation     = ProxyRotation::None;
    QTimer               *m_timer        = nullptr;
    QNetworkAccessManager m_nam;
    QMutex                m_mutex;
    QPointer<QWebEngineProfile> m_profile;

    // Health checker state
    int  m_checksPending  = 0;
    bool m_checkAborted   = false;

    // OVPN
    QString m_ovpnRemote;
    int     m_ovpnPort = 1194;

    static QVector<ProxyEntry> parseProxyText(const QString &text);

    // URL used to verify proxy is actually working (not just TCP open)
    static constexpr const char *CHECK_URL = "http://httpbin.org/ip";
    static constexpr int         CHECK_TIMEOUT_MS = 5000;
    static constexpr int         CHECK_CONCURRENCY = 20; // parallel checks
};