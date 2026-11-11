#pragma once
#include <Qt>

// macOS delivers "open this document" from the Finder as an Apple Event,
// which Qt turns into a QFileOpenEvent. QApplication::event() hands that
// event to the focus widget — but right after LaunchServices started the
// app there is no focus widget yet, so the event is dropped and the
// document never opens. Watching the event on the application object
// catches it in both cases: the app was just launched with a document, or
// it was already running and the user double-clicked another file.
//
// Two details this class has to get right:
//
//   * An event filter installed on the QApplication sees *every* event the
//     application dispatches — QCoreApplication::notify() runs the
//     application's filters for all of them, not just the ones addressed
//     to the application object. A FileOpenEvent addressed to the
//     application is then propagated up the widget hierarchy, so the same
//     document arrives dozens of times. Only the event addressed to the
//     application object itself is acted upon.
//
//   * The file can arrive either as a plain path (QFileOpenEvent::file())
//     or as a URL (QFileOpenEvent::url()); macOS uses the latter for
//     files opened from a cloud location.
//
// Header-only (no Q_OBJECT) so it can be unit-tested without pulling in
// the rest of the application — same pattern as app/TitleBarFullscreen.h.
class FileOpenWatcher : public QObject {
public:
    FileOpenWatcher(QObject* parent, std::function<void(const QString&)> onOpen)
        : QObject(parent), onOpen_(std::move(onOpen)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::FileOpen) {
            return QObject::eventFilter(watched, event);
        }
        // Only the event addressed to the application object — not the
        // copies propagated through the widget tree.
        if (watched != parent()) {
            return QObject::eventFilter(watched, event);
        }
        auto* foe = static_cast<QFileOpenEvent*>(event);
        QString path = foe->file();
        if (path.isEmpty() && foe->url().isLocalFile()) {
            path = foe->url().toLocalFile();
        }
        if (!path.isEmpty()) {
            onOpen_(path);
            return true;  // handled — don't dispatch it a second time
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void(const QString&)> onOpen_;
};

// Installs the watcher on the application object.
//
// The wiring lives here, next to the class, so that main() and the test
// use exactly the same call — a watcher that is created but never
// installed is precisely the bug this guards against (the Finder event
// then goes nowhere and the document never opens).
inline void watchFileOpenEvents(QObject* app,
                                std::function<void(const QString&)> onOpen) {
    auto* watcher = new FileOpenWatcher(app, std::move(onOpen));
    app->installEventFilter(watcher);
}
