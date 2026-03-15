#include <QApplication>
#include <QWebEngineProfile>
#include "app/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Nothing Browser");
    app.setApplicationVersion("0.1.0");

    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(18, 18, 18));
    dark.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    dark.setColor(QPalette::Base,            QColor(24, 24, 24));
    dark.setColor(QPalette::AlternateBase,   QColor(30, 30, 30));
    dark.setColor(QPalette::Text,            QColor(220, 220, 220));
    dark.setColor(QPalette::Button,          QColor(35, 35, 35));
    dark.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    dark.setColor(QPalette::Highlight,       QColor(0, 120, 215));
    dark.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(dark);

    MainWindow window;
    window.show();

    return app.exec();
}
