#include <QtTest>
#include <QThread>
#include <atomic>
#include "documents/Document.h"

class DocumentLockTest : public QObject {
    Q_OBJECT
private slots:
    void concurrent_reads_and_writes_do_not_corrupt();
};

void DocumentLockTest::concurrent_reads_and_writes_do_not_corrupt() {
    Document d;
    d.setText("seed");

    QThread writer1, writer2, reader1, reader2;
    std::atomic<int> corrupted_reads{0};

    auto run_writer = [&]() {
        for (int i = 0; i < 1000; ++i) {
            d.setText(QString("write-%1").arg(i));
        }
        QThread::currentThread()->quit();   // 让 QThread::wait() 返回
    };
    auto run_reader = [&]() {
        for (int i = 0; i < 1000; ++i) {
            QString t = d.text();
            if (!(t.isEmpty() || t.startsWith("write-") || t == "seed")) {
                ++corrupted_reads;
            }
        }
        QThread::currentThread()->quit();
    };
    QObject::connect(&writer1, &QThread::started, run_writer);
    QObject::connect(&writer2, &QThread::started, run_writer);
    QObject::connect(&reader1, &QThread::started, run_reader);
    QObject::connect(&reader2, &QThread::started, run_reader);

    writer1.start(); writer2.start(); reader1.start(); reader2.start();
    writer1.wait();  writer2.wait();
    reader1.wait();  reader2.wait();

    QCOMPARE(corrupted_reads.load(), 0);
}

QTEST_MAIN(DocumentLockTest)
#include "DocumentLockTest.moc"
