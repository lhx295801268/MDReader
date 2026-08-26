#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("MDReader");
    QApplication::setApplicationName("MDReader");
    QApplication::setApplicationVersion("0.1.0");
    MainWindow w;
    for (int i = 1; i < argc; ++i) {
        QString a = QString::fromLocal8Bit(argv[i]);
        if (!a.startsWith('-')) w.openFileFromCli(a);
    }
    w.show();
    return app.exec();
}