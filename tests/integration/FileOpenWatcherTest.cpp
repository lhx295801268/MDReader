#include <QtTest>
#include <QApplication>
#include <QFileOpenEvent>
#include <QWidget>
#include "app/FileOpenWatcher.h"

// Integration test for the Finder's "open document" path.
//
// The bug this guards against: the watcher was created but never
// installed as an event filter, so the QFileOpenEvent that macOS sends
// when the user double-clicks a .md file in the Finder went nowhere and
// the document never opened.
//
// The second case matters just as much — an event filter installed on the
// QApplication sees *every* event the app dispatches, and a FileOpenEvent
// addressed to the application is propagated up the widget hierarchy. A
// watcher that reacted to all of those would open the same document
// dozens of times.
class FileOpenWatcherTest : public QObject {
    Q_OBJECT
private slots:
    void file_open_event_on_the_application_is_handled();
    void file_open_event_on_a_widget_is_ignored();
    void url_only_event_is_handled();
};

void FileOpenWatcherTest::file_open_event_on_the_application_is_handled() {
    QStringList opened;
    watchFileOpenEvents(qApp, [&opened](const QString& p) {
        opened << p;
    });

    QFileOpenEvent event("/tmp/one.md");
    QCoreApplication::sendEvent(qApp, &event);

    QCOMPARE(opened.size(), 1);
    QCOMPARE(opened.first(), QString("/tmp/one.md"));
}

void FileOpenWatcherTest::file_open_event_on_a_widget_is_ignored() {
    QStringList opened;
    watchFileOpenEvents(qApp, [&opened](const QString& p) {
        opened << p;
    });

    // The same event, but addressed to a widget: that is the copy that
    // travels up the widget tree after the application-level one was
    // handled. Reacting to it would open the document again.
    QWidget widget;
    QFileOpenEvent event("/tmp/two.md");
    QCoreApplication::sendEvent(&widget, &event);

    QVERIFY(opened.isEmpty());
}

void FileOpenWatcherTest::url_only_event_is_handled() {
    QStringList opened;
    watchFileOpenEvents(qApp, [&opened](const QString& p) {
        opened << p;
    });

    // macOS sends a URL when the document comes from a cloud location.
    QFileOpenEvent event(QUrl::fromLocalFile("/tmp/three.md"));
    QCoreApplication::sendEvent(qApp, &event);

    QCOMPARE(opened.size(), 1);
    QCOMPARE(opened.first(), QString("/tmp/three.md"));
}

QTEST_MAIN(FileOpenWatcherTest)
#include "FileOpenWatcherTest.moc"
