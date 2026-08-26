#pragma once
#include <QObject>
#include <QHash>
#include <QFileSystemWatcher>
#include <QTimer>

class FileWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileWatcher(QObject* parent = nullptr);
    void watch(const QString& path);
    void unwatch(const QString& path);

signals:
    void externalModified(const QString& path, const QByteArray& bytes);

private slots:
    void onFsEvent(const QString& path);
    void onDebounceTimeout();

private:
    struct Entry { QTimer* timer; };
    QFileSystemWatcher* fs_ = nullptr;
    QHash<QString, Entry> entries_;
};