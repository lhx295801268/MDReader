#pragma once
#include <QCoreApplication>
#include <QFuture>
#include <QMetaObject>
#include <QtConcurrent>
#include <QThreadPool>
#include <utility>

// run work on a worker thread from QThreadPool's global pool.
// Returns QFuture<T> where T = return type of fn.
// CALLER is responsible for awaiting the returned future (waitForFinished)
// before reading results — the work runs asynchronously.
template <typename F>
auto runOnWorker(F&& fn) {
    return QtConcurrent::run(QThreadPool::globalInstance(), std::forward<F>(fn));
}

// Post work to the main thread's event loop (queued, async).
// Qt 5.10+ overload of QMetaObject::invokeMethod accepts any callable and
// serializes a copy to the queue, so we don't need to wrap in std::function
// (which would force a heap allocation).
template <typename F>
void postToMain(F&& fn) {
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        std::forward<F>(fn),
        Qt::QueuedConnection);
}
