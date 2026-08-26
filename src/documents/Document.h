#pragma once
#include <QString>
#include <shared_mutex>

// Document is a shared state object — multiple readers (editor, preview,
// render coordinator, file watcher) may hold a pointer/reference while a
// single writer (editor on user input) mutates it.
//
// `std::shared_mutex` is non-copyable/non-movable, so Document is too —
// it must always live behind a pointer or reference (no value semantics).
class Document {
public:
    Document();
    explicit Document(QString path, QString initialText = QString());

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

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
