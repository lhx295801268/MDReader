#include <QtTest>
#include <QThread>
#include <QCoreApplication>
#include <atomic>
#include <thread>
#include "services/WorkerThread.h"

class WorkerThreadTest : public QObject {
    Q_OBJECT
private slots:
    void runOnWorker_executes_on_different_thread();
    void postToMain_executes_on_main_thread();
};

void WorkerThreadTest::runOnWorker_executes_on_different_thread() {
    auto mainTid = std::this_thread::get_id();
    std::atomic<std::thread::id> workerTid;
    QEventLoop loop;
    QFuture<void> f = runOnWorker([&] {
        workerTid = std::this_thread::get_id();
        QMetaObject::invokeMethod(&loop, "quit", Qt::QueuedConnection);
    });
    loop.exec();
    f.waitForFinished();
    QVERIFY(workerTid.load() != mainTid);
}

void WorkerThreadTest::postToMain_executes_on_main_thread() {
    auto mainTid = std::this_thread::get_id();
    std::atomic<std::thread::id> deliveredTid;
    QEventLoop loop;
    postToMain([&] {
        deliveredTid = std::this_thread::get_id();
        loop.quit();
    });
    loop.exec();
    QVERIFY(deliveredTid.load() == mainTid);
}

QTEST_MAIN(WorkerThreadTest)
#include "WorkerThreadTest.moc"
