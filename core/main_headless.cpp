#include <QApplication>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>
#include <QCoreApplication>
#include "engine/piggy/PiggyServer.h"
#include "engine/piggy/Sessionmanager.h"

// ── Key generation ────────────────────────────────────────────────────────────

static QString generateKey() {
    QString a = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    QString b = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    return "peaseernest" + (a + b).left(53);
}

// Key file lives in cwd, not in binary dir, so each project gets its own key.
static QString keyFilePath(const QString &name) {
    return SessionManager::workDir() + "/" + name + ".piggy";
}

static std::pair<QString, QString> loadExistingKey() {
    QDir dir(SessionManager::workDir());
    QStringList files = dir.entryList({"*.piggy"}, QDir::Files);
    if (files.isEmpty()) return {"", ""};
    QFile f(dir.filePath(files.first()));
    if (!f.open(QIODevice::ReadOnly)) return {"", ""};
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    return {obj["name"].toString(), obj["key"].toString()};
}

static std::tuple<QString, QString, QString> firstRunSetup() {
    QTextStream in(stdin);
    QTextStream out(stdout);

    out << "\n";
    out << "  ██████╗ ██╗ ██████╗  ██████╗ ██╗   ██╗\n";
    out << "  ██╔══██╗██║██╔════╝ ██╔════╝ ╚██╗ ██╔╝\n";
    out << "  ██████╔╝██║██║  ███╗██║  ███╗ ╚████╔╝ \n";
    out << "  ██╔═══╝ ██║██║   ██║██║   ██║  ╚██╔╝  \n";
    out << "  ██║     ██║╚██████╔╝╚██████╔╝   ██║   \n";
    out << "  ╚═╝     ╚═╝ ╚═════╝  ╚═════╝    ╚═╝   \n";
    out << "  Headless Browser Daemon\n";
    out << "  Working dir: " << SessionManager::workDir() << "\n\n";

    out << "Mode? (socket/http): ";
    out.flush();
    QString mode = in.readLine().trimmed().toLower();
    if (mode != "http" && mode != "socket") { mode = "socket"; }

    if (mode == "socket") {
        out << "\n[Piggy] Starting in socket mode...\n";
        out.flush();
        return {"socket", "", ""};
    }

    out << "Session name: ";
    out.flush();
    QString name = in.readLine().trimmed();
    if (name.isEmpty()) name = "default";

    QString key  = generateKey();
    QString path = keyFilePath(name);

    QJsonObject obj;
    obj["name"]    = name;
    obj["key"]     = key;
    obj["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();

    out << "\n";
    out << "  Session : " << name << "\n";
    out << "  Key     : " << key  << "\n";
    out << "  Saved to: " << path << "\n\n";
    out << "  Data files will be in: " << SessionManager::workDir() << "\n";
    out << "    cookies.json  — persistent cookies (always on)\n";
    out << "    profile.json  — browser identity (always on)\n";
    out << "    ws.json       — WebSocket frames (opt-in)\n";
    out << "    pings.json    — ping log (opt-in)\n\n";
    out.flush();

    return {"http", name, key};
}

int main(int argc, char *argv[]) {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        "--disable-gpu "
        "--no-sandbox "
        "--disable-dev-shm-usage "
        "--disable-software-rasterizer "
        "--headless=new "
        "--allow-running-insecure-content "
        "--disable-site-isolation-trials "
        "--disable-features=IsolateOrigins,WebRtcHideLocalIpsWithMdns "
        "--js-flags=--max-old-space-size=512"
    );
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    app.setApplicationName("nothing-browser-headless");

    QString mode, name, key;
    auto [ename, ekey] = loadExistingKey();

    if (!ekey.isEmpty()) {
        mode = "http";
        name = ename;
        key  = ekey;
        qInfo() << "[Piggy] Session:" << name;
        qInfo() << "[Piggy] Working dir:" << SessionManager::workDir();
        qInfo() << "[Piggy] HTTP mode — port 2005";
    } else {
        auto [m, n, k] = firstRunSetup();
        mode = m; name = n; key = k;
    }

    auto *profile = new QWebEngineProfile(&app);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    auto *defaultPage = new QWebEnginePage(profile, &app);

    PiggyServer server(defaultPage, &app);
    server.start();

    if (mode == "http") {
        server.startHttp(key);
        qInfo() << "[Piggy] HTTP API ready on port 2005";
    } else {
        qInfo() << "[Piggy] Socket ready:" << PiggyServer::SOCKET_NAME;
    }

    return app.exec();
}