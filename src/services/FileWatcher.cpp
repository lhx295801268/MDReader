#include "services/FileWatcher.h"
#include <QFile>
#include <QtConcurrent>
#include <QThreadPool>
#include <QPointer>

FileWatcher::FileWatcher(QObject* parent) : QObject(parent) {
    fs_ = new QFileSystemWatcher(this);
    connect(fs_, &QFileSystemWatcher::fileChanged, this, &FileWatcher::onFsEvent);
}

void FileWatcher::watch(const QString& path) {
    if (entries_.contains(path)) return;
    Entry e; e.timer = new QTimer(this);
    e.timer->setSingleShot(true); e.timer->setInterval(500);
    connect(e.timer, &QTimer::timeout, this, &FileWatcher::onDebounceTimeout);
    entries_.insert(path, e);
    fs_->addPath(path);
}

void FileWatcher::unwatch(const QString& path) {
    auto it = entries_.find(path);
    if (it == entries_.end()) return;
    delete it->timer; entries_.erase(it);
    fs_->removePath(path);
}

void FileWatcher::onFsEvent(const QString& path) {
    auto it = entries_.find(path);
    if (it != entries_.end()) it->timer->start();
}

void FileWatcher::onDebounceTimeout() {
    auto* t = qobject_cast<QTimer*>(sender());
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->timer == t) {
            QString path = it.key();
            QtConcurrent::run(QThreadPool::globalInstance(), [this, path] {
                QFile f(path);
                QByteArray bytes;
                if (f.open(QIODevice::ReadOnly)) bytes = f.readAll();
                QPointer<FileWatcher> self(this);
                QMetaObject::invokeMethod(this, [self, path, bytes]() {
                    if (!self) return;
                    if (!self->entries_.contains(path)) return;   // unwatched while worker was in flight
                    emit self->externalModified(path, bytes);
                }, Qt::QueuedConnection);
            });
            break;
        }
    }
}