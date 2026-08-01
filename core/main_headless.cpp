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
#include "../engine/piggy/PiggyServer.h"

// ── Key generation — prefixed with "peaseernest" then 52 random hex chars ────

static QString generateKey(const QString &name) {
    QString a = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    QString b = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    QString random = (a + b).left(53); // 53 random chars
    return "peaseernest" + random;     // "peaseernest" = 11 chars + 53 = 64 total
}

static QString keyFilePath(const QString &name) {
    return QDir(QCoreApplication::applicationDirPath()).filePath(name + ".piggy");
}

static std::pair<QString, QString> loadExistingKey() {
    QDir dir(QCoreApplication::applicationDirPath());
    QStringList files = dir.entryList({"*.piggy"}, QDir::Files);
    if (files.isEmpty()) return {"", ""};
    QFile f(dir.filePath(files.first()));
    if (!f.open(QIODevice::ReadOnly)) return {"", ""};
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    return {obj["name"].toString(), obj["key"].toString()};
}

// Every script talks to the exact same ws://host:2005 endpoint no matter
// what — the only thing that varies is whether a key is required. So setup
// now just asks "open or key-protected?" instead of the old "socket/http"
// mode split, which no longer exists as a real distinction.
static std::pair<QString, QString> firstRunSetup() {
    QTextStream in(stdin);
    QTextStream out(stdout);

    out << "\n";
    out << "  ██████╗ ██╗ ██████╗  ██████╗ ██╗   ██╗\n";
    out << "  ██╔══██╗██║██╔════╝ ██╔════╝ ╚██╗ ██╔╝\n";
    out << "  ██████╔╝██║██║  ███╗██║  ███╗ ╚████╔╝ \n";
    out << "  ██╔═══╝ ██║██║   ██║██║   ██║  ╚██╔╝  \n";
    out << "  ██║     ██║╚██████╔╝╚██████╔╝   ██║   \n";
    out << "  ╚═╝     ╚═╝ ╚═════╝  ╚═════╝    ╚═╝   \n";
    out << "  Headless Browser Daemon\n\n";

    out << "Require a connection key? This only matters if you're exposing\n";
    out << "this to the network — local scripts on this machine can always\n";
    out << "connect over 127.0.0.1 either way. (y/N): ";
    out.flush();
    QString wantsKey = in.readLine().trimmed().toLower();

    if (wantsKey != "y" && wantsKey != "yes") {
        out << "\n[Piggy] Starting open — no key required\n";
        out.flush();
        return {"", ""};
    }

    out << "Session name: ";
    out.flush();
    QString name = in.readLine().trimmed();
    if (name.isEmpty()) name = "default";

    QString key  = generateKey(name);
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
    out << "  Saved to: " << path << "\n";
    out << "\n";
    out << "  Keep your key safe — it will not be shown again.\n";
    out << "  To reset: delete " << path << " and restart.\n\n";
    out.flush();

    return {name, key};
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

    QString name, key;
    auto [ename, ekey] = loadExistingKey();

    if (!ekey.isEmpty()) {
        name = ename;
        key  = ekey;
        qInfo() << "[Piggy] Loaded session:" << name;
    } else {
        auto [n, k] = firstRunSetup();
        name = n; key = k;
    }

    auto *profile = new QWebEngineProfile(&app);
    profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    auto *defaultPage = new QWebEnginePage(profile, &app);

    PiggyServer server(defaultPage, &app);
    server.start(key); // key may be empty — that's fine, means "open"

    if (!key.isEmpty()) {
        qInfo() << "[Piggy] Session:" << name << "— key required to connect";
    }

    return app.exec();
}
