#pragma once
#include <QFuture>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QThreadPool>
#include <functional>
#include <type_traits>
#include <utility>

// run work on a worker thread from QThreadPool's global pool.
// Returns QFuture<T> where T = return type of fn.
template <typename F>
auto runOnWorker(F&& fn) {
    return QtConcurrent::run(QThreadPool::globalInstance(), std::forward<F>(fn));
}

// Post work to the main thread's event loop (queued, async).
inline void postToMain(std::function<void()> fn) {
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [fn = std::move(fn)]() { fn(); },
        Qt::QueuedConnection);
}
