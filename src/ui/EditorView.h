#pragma once
#include <QPlainTextEdit>
#include "documents/Document.h"
#include "app/EditorPalette.h"

class EditorView : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit EditorView(QWidget* parent = nullptr);
    void setDocument(std::shared_ptr<Document> doc);
    std::shared_ptr<Document> document() const { return doc_; }

    void setShowLineNumbers(bool on);
    bool showLineNumbers() const { return showLineNumbers_; }

    // Phase 12: switch the editor's background / foreground / gutter colors
    // to match the active preview theme. Caller passes a resolved
    // mdreader::theme::EditorPalette (header-only lookup in
    // app/EditorPalette.h). Idempotent — applying the same palette twice
    // is a no-op. The gutter (line-number strip) is repainted immediately
    // so the color flip is visible without a resize or text change.
    void applyEditorPalette(const mdreader::theme::EditorPalette& palette);

    // Bulk-replace editor text from disk (Phase 4 external-reload path).
    // Distinct from syncFromDocument(): the source string comes from the
    // caller, not from doc_->text(). The name keeps the intent explicit —
    // we are forcing a view-only update, not a doc → editor sync round-trip.
    void setTextDirect(const QString& text) { setPlainText(text); }

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
    // Phase 12: cached line-number paint colors so paintLineNumbers() can
    // pick them up at draw time without re-asking the palette. Updated by
    // applyEditorPalette(); defaults match the github light theme so a
    // constructor-time paint (before any theme has been applied) is still
    // legible.
    QColor lineNumberBgCached_ = QColor(245, 245, 245);
    QColor lineNumberFgCached_ = QColor(150, 150, 150);
};
