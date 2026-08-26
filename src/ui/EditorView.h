#pragma once
#include <QPlainTextEdit>
#include "documents/Document.h"

class EditorView : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit EditorView(QWidget* parent = nullptr);
    void setDocument(std::shared_ptr<Document> doc);
    std::shared_ptr<Document> document() const { return doc_; }

    void setShowLineNumbers(bool on);
    bool showLineNumbers() const { return showLineNumbers_; }

    // Helpers used by LineNumberArea (a QWidget child defined in the .cpp).
    // Public so the nested LineNumberArea class can call them.
    int lineNumberAreaWidth();
    void paintLineNumbers(QPaintEvent* event);

public slots:
    void syncFromDocument();  // 把 Document::text() 推回 editor
    void syncToDocument();    // 把 editor 文本写回 Document

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect& rect, int dy);

protected:
    void resizeEvent(QResizeEvent* e) override;
    bool canInsertFromMimeData(const QMimeData* source) const override;
    void insertFromMimeData(const QMimeData* source) override;

private:
    QWidget* lineNumberArea_ = nullptr;
    std::shared_ptr<Document> doc_;
    bool showLineNumbers_ = true;
    bool loadingFromDoc_ = false;
};
