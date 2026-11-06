#include <QApplication>
#include <QIcon>
#include <QTextStream>
#include "app/MainWindow.h"

// Injected by CMake (see src/CMakeLists.txt). The fallback keeps this
// file compilable outside that build system.
#ifndef MDREADER_VERSION
#define MDREADER_VERSION "0.0.0"
#endif

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("MDReader");
    QApplication::setApplicationName("MDReader");
    // The display name is what macOS shows in the application menu (the
    // left-most menu of the system menu bar) and in the window title.
    QApplication::setApplicationDisplayName(QStringLiteral("MDReader"));
    QApplication::setApplicationVersion(QStringLiteral(MDREADER_VERSION));
    // Window / task-bar icon. On macOS the real icon comes from the .app
    // bundle (CFBundleIconFile → packaging/mdreader.icns); this PNG is
    // the equivalent for Linux and Windows.
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/mdreader-1024.png")));

    // --version prints the version and exits — handy for packaging
    // scripts and for checking which build is on the PATH.
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "-v" || a == "--version") {
            QTextStream out(stdout);
            out << "MDReader " << MDREADER_VERSION << "\n";
            return 0;
        }
    }

    MainWindow w;
    // Remaining non-option arguments are files to open. On macOS the
    // system may also pass a "-psn_..." process-serial argument; the
    // leading '-' check skips it. Files opened by Finder while the app is
    // already running arrive as a QFileOpenEvent instead — MainWindow
    // handles that in its event() override.
    for (int i = 1; i < argc; ++i) {
        QString a = QString::fromLocal8Bit(argv[i]);
        if (!a.startsWith('-')) w.openFileFromCli(a);
    }
    w.show();
    return app.exec();
}
