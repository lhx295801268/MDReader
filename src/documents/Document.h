#pragma once
#include <QString>
#include <shared_mutex>

class Document {
public:
    Document();
    explicit Document(QString path, QString initialText = QString());

    QString path() const;
    QString text() const;        // 返回副本
    void setText(QString s);
    bool dirty() const;
    void markSaved();

private:
    QString path_;
    QString text_;
    bool dirty_ = false;
    mutable std::shared_mutex mtx_;
};