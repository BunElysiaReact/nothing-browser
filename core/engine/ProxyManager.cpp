#include "ProxyManager.h"
#include <QWebEngineProfile>
#include <QNetworkRequest>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMutexLocker>
#include <QUrl>
#include <QElapsedTimer>

// ═════════════════════════════════════════════════════════════════════════════
//  ProxyEntry helpers
// ═════════════════════════════════════════════════════════════════════════════

QString ProxyEntry::toString() const {
    QString scheme = (type == HTTP) ? "http" : (type == HTTPS ? "https" : "socks5");
    QString s = scheme + "://";
    if (!user.isEmpty()) {
        s += user;
        if (!pass.isEmpty()) s += ":" + pass;
        s += "@";
    }
    s += host + ":" + QString::number(port);
    return s;
}

ProxyEntry ProxyEntry::fromString(const QString &raw) {
    ProxyEntry e;
    QString line = raw.trimmed();
    if (line.isEmpty() || line.startsWith('#')) return e;

    if (line.startsWith("socks5://", Qt::CaseInsensitive)) {
        e.type = SOCKS5; line = line.mid(9);
    } else if (line.startsWith("socks4://", Qt::CaseInsensitive)) {
        e.type = SOCKS5; line = line.mid(9);
    } else if (line.startsWith("https://", Qt::CaseInsensitive)) {
        e.type = HTTPS; line = line.mid(8);
    } else if (line.startsWith("http://", Qt::CaseInsensitive)) {
        e.type = HTTP; line = line.mid(7);
    } else {
        e.type = SOCKS5;
    }

    if (line.contains('@')) {
        auto parts = line.split('@');
        auto creds = parts[0].split(':');
        e.user = creds.value(0);
        e.pass = creds.value(1);
        line   = parts[1];
    }

    auto parts = line.split(':');
    if (parts.size() >= 2) {
        e.host = parts[0];
        e.port = parts[1].toUShort();
        if (parts.size() >= 4) { e.user = parts[2]; e.pass = parts[3]; }
    } else {
        e.host = line; e.port = 1080;
    }

    return e;
}

QNetworkProxy ProxyEntry::toQProxy() const {
    QNetworkProxy::ProxyType qt;
    switch (type) {
        case HTTP:  case HTTPS: qt = QNetworkProxy::HttpProxy;   break;
        default:                qt = QNetworkProxy::Socks5Proxy; break;
    }
    QNetworkProxy p(qt, host, port);
    if (!user.isEmpty()) p.setUser(user);
    if (!pass.isEmpty()) p.setPassword(pass);
    return p;
}

// ═════════════════════════════════════════════════════════════════════════════
//  ProxyManager
// ═════════════════════════════════════════════════════════════════════════════

ProxyManager &ProxyManager::instance() {
    static ProxyManager s;
    return s;
}

ProxyManager::ProxyManager(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProxyManager::rotate);
}

QVector<ProxyEntry> ProxyManager::parseProxyText(const QString &text) {
    QVector<ProxyEntry> list;
    for (const QString &line : text.split('\n')) {
        ProxyEntry e = ProxyEntry::fromString(line.trimmed());
        if (e.isValid()) list.append(e);
    }
    return list;
}

// ── Load / fetch ──────────────────────────────────────────────────────────────

bool ProxyManager::loadFromFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QMutexLocker lock(&m_mutex);
    m_proxies = parseProxyText(QString::fromUtf8(f.readAll()));
    m_index   = 0;
    lock.unlock();
    emit proxyListLoaded(m_proxies.size());
    if (!m_proxies.isEmpty()) { m_active = true; applyEntry(m_proxies[0]); }
    if (m_autoCheck) QTimer::singleShot(0, this, &ProxyManager::checkAll);
    return !m_proxies.isEmpty();
}

void ProxyManager::fetchFromUrl(const QString &url) {
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "NothingBrowser/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchFailed(reply->errorString()); return;
        }
        QMutexLocker lock(&m_mutex);
        m_proxies = parseProxyText(QString::fromUtf8(reply->readAll()));
        m_index   = 0;
        lock.unlock();
        emit proxyListLoaded(m_proxies.size());
        if (!m_proxies.isEmpty()) { m_active = true; applyEntry(m_proxies[0]); }
        if (m_autoCheck) QTimer::singleShot(0, this, &ProxyManager::checkAll);
    });
}

bool ProxyManager::loadOvpnFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QString remoteHost; int remotePort = 1194; bool isTcp = false;
    QString authUser, authPass; bool inCreds = false;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("remote ", Qt::CaseInsensitive)) {
            auto p = line.split(QRegularExpression("\\s+"));
            if (p.size() >= 3) { remoteHost = p[1]; remotePort = p[2].toInt();
                if (p.size() >= 4) isTcp = (p[3].toLower() == "tcp"); }
        } else if (line.startsWith("proto ", Qt::CaseInsensitive)) {
            isTcp = line.contains("tcp", Qt::CaseInsensitive);
        } else if (line == "<auth-user-pass>") { inCreds = true; }
        else if (line == "</auth-user-pass>") { inCreds = false; }
        else if (inCreds) {
            if (authUser.isEmpty()) authUser = line;
            else if (authPass.isEmpty()) authPass = line;
        }
    }
    if (remoteHost.isEmpty()) return false;
    m_ovpnRemote = remoteHost; m_ovpnPort = remotePort;
    QMutexLocker lock(&m_mutex);
    m_proxies.clear();
    ProxyEntry local; local.type = ProxyEntry::SOCKS5;
    local.host = "127.0.0.1"; local.port = 1080;
    m_proxies.append(local);
    if (isTcp && !remoteHost.isEmpty()) {
        ProxyEntry remote; remote.type = ProxyEntry::HTTP;
        remote.host = remoteHost; remote.port = (quint16)remotePort;
        remote.user = authUser;   remote.pass = authPass;
        m_proxies.append(remote);
    }
    m_index = 0; m_active = true;
    lock.unlock();
    emit ovpnLoaded(remoteHost, remotePort);
    emit proxyListLoaded(m_proxies.size());
    applyEntry(local);
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Health Checker
//
//  Strategy: for each proxy, create a QNetworkAccessManager configured to
//  use that specific proxy, then GET httpbin.org/ip with a 5s timeout.
//  - Success + HTTP 200 → Alive, record latency
//  - Timeout / connection refused / non-200 → Dead
//
//  We fire up to CHECK_CONCURRENCY requests at once, then batch the rest
//  as slots free up. This way 500 proxies completes in ~25s not 2500s.
// ═════════════════════════════════════════════════════════════════════════════

void ProxyManager::checkAll() {
    QMutexLocker lock(&m_mutex);
    int total = m_proxies.size();
    if (total == 0) return;

    // Reset all to Unchecked
    for (auto &e : m_proxies) { e.health = ProxyEntry::Unchecked; e.latency = -1; }
    lock.unlock();

    m_checkAborted   = false;
    m_checksPending  = 0;
    emit checkStarted(total);

    // Fire first batch
    int batch = qMin(total, CHECK_CONCURRENCY);
    for (int i = 0; i < batch; i++) doCheckOne(i);
}

void ProxyManager::checkOne(int index) {
    QMutexLocker lock(&m_mutex);
    if (index < 0 || index >= m_proxies.size()) return;
    m_proxies[index].health  = ProxyEntry::Unchecked;
    m_proxies[index].latency = -1;
    lock.unlock();
    m_checkAborted = false;
    emit checkStarted(1);
    doCheckOne(index);
}

void ProxyManager::stopChecking() {
    m_checkAborted = true;
}

void ProxyManager::doCheckOne(int index) {
    if (m_checkAborted) return;

    QMutexLocker lock(&m_mutex);
    if (index < 0 || index >= m_proxies.size()) return;
    m_proxies[index].health = ProxyEntry::Checking;
    ProxyEntry entry = m_proxies[index];
    lock.unlock();

    m_checksPending++;

    // Each check gets its own NAM configured with this proxy
    auto *nam = new QNetworkAccessManager(this);
    nam->setProxy(entry.toQProxy());

    QNetworkRequest req{QUrl(CHECK_URL)};
    req.setTransferTimeout(CHECK_TIMEOUT_MS);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (X11; Linux x86_64) Chrome/124.0.0.0 Safari/537.36");

    auto *timer = new QElapsedTimer();
    timer->start();

    auto *reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, nam, timer, index]() mutable {
            reply->deleteLater();
            nam->deleteLater();

            int latency = (int)timer->elapsed();
            delete timer;

            ProxyEntry::Health result;

            if (m_checkAborted) {
                m_checksPending--;
                return;
            }

            bool ok = (reply->error() == QNetworkReply::NoError)
                   && (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);
            result  = ok ? ProxyEntry::Alive : ProxyEntry::Dead;

            {
                QMutexLocker lock(&m_mutex);
                if (index < m_proxies.size()) {
                    m_proxies[index].health  = result;
                    m_proxies[index].latency = ok ? latency : -1;
                }
            }

            emit checkProgress(index, result, ok ? latency : -1);

            m_checksPending--;

            // ── Fire next proxy in queue ──────────────────────────────────────
            // Find the next Unchecked entry to maintain concurrency
            {
                QMutexLocker lock(&m_mutex);
                for (int i = 0; i < m_proxies.size(); i++) {
                    if (m_proxies[i].health == ProxyEntry::Unchecked) {
                        lock.unlock();
                        doCheckOne(i);
                        goto done;
                    }
                }
            }

            // ── All done ──────────────────────────────────────────────────────
            if (m_checksPending == 0 && !m_checkAborted) {
                emit checkFinished(aliveCount(), deadCount());

                // If current proxy is dead, advance to first alive one
                if (m_skipDead) {
                    QMutexLocker lock(&m_mutex);
                    if (m_index < m_proxies.size() &&
                        m_proxies[m_index].health == ProxyEntry::Dead)
                    {
                        for (int i = 0; i < m_proxies.size(); i++) {
                            if (m_proxies[i].health == ProxyEntry::Alive) {
                                m_index = i;
                                ProxyEntry e = m_proxies[i];
                                lock.unlock();
                                applyEntry(e);
                                break;
                            }
                        }
                    }
                }
            }
            done:;
        }
    );
}

// ── Queries ───────────────────────────────────────────────────────────────────

int ProxyManager::aliveCount() const {
    QMutexLocker lock(const_cast<QMutex*>(&m_mutex));
    int n = 0;
    for (auto &e : m_proxies) if (e.health == ProxyEntry::Alive) n++;
    return n;
}

int ProxyManager::deadCount() const {
    QMutexLocker lock(const_cast<QMutex*>(&m_mutex));
    int n = 0;
    for (auto &e : m_proxies) if (e.health == ProxyEntry::Dead) n++;
    return n;
}

// ── Rotation ──────────────────────────────────────────────────────────────────

void ProxyManager::setRotation(ProxyRotation mode, int intervalSecs) {
    m_rotation = mode;
    m_timer->stop();
    if (mode == ProxyRotation::Timed && intervalSecs > 0)
        m_timer->start(intervalSecs * 1000);
}

void ProxyManager::rotate() {
    QMutexLocker lock(&m_mutex);
    if (m_proxies.isEmpty()) return;

    // Skip dead proxies if skipDead is on
    int start = m_index;
    do {
        m_index = (m_index + 1) % m_proxies.size();
        if (!m_skipDead || m_proxies[m_index].isUsable()) break;
        // Prevent infinite loop if all are dead
    } while (m_index != start);

    ProxyEntry e = m_proxies[m_index];
    lock.unlock();
    applyEntry(e);
}

void ProxyManager::next() { rotate(); }

void ProxyManager::disable() {
    m_active = false;
    m_timer->stop();
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    emit proxyChanged(ProxyEntry{});
}

void ProxyManager::enableCurrent() {
    QMutexLocker lock(&m_mutex);
    if (m_proxies.isEmpty()) return;
    ProxyEntry e = m_proxies[m_index];
    lock.unlock();
    m_active = true;
    applyEntry(e);
}

void ProxyManager::applyToProfile(QWebEngineProfile *profile) {
    m_profile = profile;
    if (!m_proxies.isEmpty()) applyEntry(m_proxies[m_index]);
}

void ProxyManager::applyEntry(const ProxyEntry &e) {
    if (!e.isValid()) return;
    QNetworkProxy::setApplicationProxy(e.toQProxy());
    emit proxyChanged(e);
    qDebug() << "[ProxyManager] Active proxy:" << e.toString()
             << "| latency:" << (e.latency >= 0 ? QString::number(e.latency)+"ms" : "?");
}

ProxyEntry ProxyManager::current() const {
    QMutexLocker lock(const_cast<QMutex*>(&m_mutex));
    if (m_proxies.isEmpty() || m_index >= m_proxies.size()) return {};
    return m_proxies[m_index];
}