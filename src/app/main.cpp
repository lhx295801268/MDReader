#include <QApplication>
#include <QFileOpenEvent>
#include <QIcon>
#include <QTextStream>
#include <functional>
#include "app/MainWindow.h"

// Injected by CMake (see src/CMakeLists.txt). The fallback keeps this
// file compilable outside that build system.
#ifndef MDREADER_VERSION
#define MDREADER_VERSION "0.0.0"
#endif

// macOS delivers "open this document" from the Finder as an Apple Event,
// which Qt turns into a QFileOpenEvent. QApplication::event() hands that
// event to the focus widget — but right after LaunchServices started the
// app there is no focus widget yet, so the event is dropped and the
// document never opens. Watching the event on the application object
// catches it in both cases: the app was just launched with a document, or
// it was already running and the user double-clicked another file.
class FileOpenWatcher : public QObject {
public:
    FileOpenWatcher(QObject* parent, std::function<void(const QString&)> onOpen)
        : QObject(parent), onOpen_(std::move(onOpen)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::FileOpen) {
            auto* foe = static_cast<QFileOpenEvent*>(event);
            QString path = foe->file();
            if (path.isEmpty() && foe->url().isLocalFile()) {
                path = foe->url().toLocalFile();
            }
            if (!path.isEmpty()) {
                onOpen_(path);
                return true;  // handled — don't dispatch it a second time
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void(const QString&)> onOpen_;
};

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
    // Watch for the Finder's "open document" Apple Event on the
    // application object (see FileOpenWatcher above).
    auto* watcher = new FileOpenWatcher(&app, [&w](const QString& path) {
        w.openFileFromCli(path);
    });
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
