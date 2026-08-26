#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QFile>
#include <QThread>
#include "services/FileWatcher.h"

class FileWatcherDebounceTest : public QObject {
    Q_OBJECT
private slots:
    void five_events_within_300ms_emit_once();
    void unwatch_before_debounce_suppresses_emit();
};

void FileWatcherDebounceTest::five_events_within_300ms_emit_once() {
    QTemporaryDir tmp;
    QString path = tmp.path() + "/x.md";
    QFile f(path); f.open(QIODevice::WriteOnly); f.write("a"); f.close();

    FileWatcher fw;
    QSignalSpy spy(&fw, &FileWatcher::externalModified);
    fw.watch(path);
    for (int i = 0; i < 5; ++i) {
        QFile g(path); g.open(QIODevice::WriteOnly); g.write(QByteArray("v") + char('0' + i)); g.close();
        QThread::msleep(40);
    }
    QTest::qWait(800);
    QCOMPARE(spy.count(), 1);
}

void FileWatcherDebounceTest::unwatch_before_debounce_suppresses_emit() {
    QTemporaryDir tmp;
    QString path = tmp.path() + "/y.md";
    QFile f(path); f.open(QIODevice::WriteOnly); f.write("a"); f.close();

    FileWatcher fw;
    QSignalSpy spy(&fw, &FileWatcher::externalModified);
    fw.watch(path);
    QFile g(path); g.open(QIODevice::WriteOnly); g.write("b"); g.close();
    fw.unwatch(path);  // cancels before debounce timer fires
    QTest::qWait(800);
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(FileWatcherDebounceTest)
#include "FileWatcherDebounceTest.moc"