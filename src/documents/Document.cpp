#include "documents/Document.h"

Document::Document() = default;

Document::Document(QString path, QString initialText)
    : path_(std::move(path)), text_(std::move(initialText)) {}

QString Document::path() const {
    std::shared_lock lk(mtx_);
    return path_;
}

QString Document::text() const {
    std::shared_lock lk(mtx_);
    return text_;
}

void Document::setText(QString s) {
    std::unique_lock lk(mtx_);
    text_ = std::move(s);
    dirty_ = true;
}

bool Document::dirty() const {
    std::shared_lock lk(mtx_);
    return dirty_;
}

void Document::markSaved() {
    std::unique_lock lk(mtx_);
    dirty_ = false;
}
