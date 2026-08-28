#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <memory>

class Document;

/// Owns Document instances on the main thread. File I/O is dispatched to
/// WorkerThread; all mutations of `docs_` happen on the main thread via
/// postToMain. Lifetimes are guarded with QPointer to survive shutdown races.
class DocumentManager : public QObject {
    Q_OBJECT
public:
    explicit DocumentManager(QObject* parent = nullptr);

    // open 走 worker 读盘;完成后 emit documentLoaded.
    void openFile(const QString& path);   // 主线程入口,立即返回
    // Returns the existing Document whose absolute path matches `path`,
    // or nullptr if none is currently loaded. Callers (e.g. MainWindow)
    // use this to avoid creating duplicate tabs when the user opens a file
    // that's already on screen — e.g. double-clicking a .md that's in the
    // tab bar, or restoring session files. Path comparison is done via
    // QFileInfo::absoluteFilePath() so symlinks and relative paths resolve
    // consistently with how openFile() canonicalizes the path.
    std::shared_ptr<Document> findByPath(const QString& path) const;
    std::shared_ptr<Document> addNew(const QString& initialText = QString());
    bool saveDocument(std::shared_ptr<Document> doc);   // 返回成功;主线程同步写盘
    void closeDocument(std::shared_ptr<Document> doc);

    QList<std::shared_ptr<Document>> documents() const { return docs_; }

signals:
    void documentAdded(std::shared_ptr<Document> doc);
    void documentClosed(std::shared_ptr<Document> doc);
    void documentLoaded(std::shared_ptr<Document> doc);  // worker 读盘完成

private:
    QList<std::shared_ptr<Document>> docs_;
};