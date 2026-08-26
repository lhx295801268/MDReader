#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <memory>

class Document;

class DocumentManager : public QObject {
    Q_OBJECT
public:
    explicit DocumentManager(QObject* parent = nullptr);

    // open 走 worker 读盘;完成后 emit documentLoaded.
    void openFile(const QString& path);   // 主线程入口,立即返回
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