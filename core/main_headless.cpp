#include <QCoreApplication>
#include "PiggyServer.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("nothing-browser-headless");

    PiggyServer server(nullptr); // no PiggyTab — headless
    server.start();

    qDebug() << "[Piggy] Headless daemon running on socket: piggy";
    return app.exec();
}