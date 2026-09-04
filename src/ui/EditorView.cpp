#include "ui/EditorView.h"
#include <QPainter>
#include <QPalette>
#include <QMimeData>
#include <QImage>
#include <QBuffer>
#include <QIODevice>
#include <QFileInfo>
#include <QDir>
#include <QTextBlock>
#include "services/ImageHandler.h"

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(EditorView* editor) : QWidget(editor), editor_(editor) {}
    QSize sizeHint() const override {
        return QSize(editor_->lineNumberAreaWidth(), 0);
    }
protected:
    void paintEvent(QPaintEvent* e) override { editor_->paintLineNumbers(e); }
private:
    EditorView* editor_;
};

EditorView::EditorView(QWidget* parent) : QPlainTextEdit(parent) {
    lineNumberArea_ = new LineNumberArea(this);
    setFont(QFont("Menlo, Consolas, monospace", 11));
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &EditorView::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &EditorView::updateLineNumberArea);
    updateLineNumberAreaWidth(0);
}

int EditorView::lineNumberAreaWidth() {
    if (!showLineNumbers_) return 0;
    int digits = 1;
    for (int n = std::max(1, blockCount()); n /= 10; ++digits) {}
    return 8 + fontMetrics().horizontalAdvance('9') * digits;
}

void EditorView::setShowLineNumbers(bool on) {
    showLineNumbers_ = on;
    lineNumberArea_->setVisible(on);
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void EditorView::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void EditorView::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) lineNumberArea_->scroll(0, dy);
    else lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void EditorView::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(),
                                       lineNumberAreaWidth(), cr.height()));
}

void EditorView::paintLineNumbers(QPaintEvent* e) {
    if (!showLineNumbers_) return;
    QPainter p(lineNumberArea_);
    // Phase 12: use cached palette colors so the gutter matches the active
    // theme instead of the hard-coded #f5f5f5 / #969696 that existed
    // before. Defaults (set in the member initializer) keep the first
    // paint legible even before applyEditorPalette() has been called.
    p.fillRect(lineNumberArea_->rect(), lineNumberBgCached_);
    QTextBlock block = firstVisibleBlock();
    int top = blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + blockBoundingRect(block).height();
    int current = block.blockNumber() + 1;
    QFont f = font(); f.setBold(false);
    p.setFont(f); p.setPen(lineNumberFgCached_);
    while (block.isValid() && top <= e->rect().bottom()) {
        if (block.isVisible() && bottom >= e->rect().top()) {
            p.drawText(0, top, lineNumberArea_->width() - 2,
                       fontMetrics().height(),
                       Qt::AlignRight, QString::number(current));
        }
        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
        ++current;
    }
}

void EditorView::setDocument(std::shared_ptr<Document> doc) {
    doc_ = std::move(doc);
    syncFromDocument();
}

void EditorView::syncFromDocument() {
    if (!doc_) return;
    QString t = doc_->text();
    loadingFromDoc_ = true;
    setPlainText(t);
    loadingFromDoc_ = false;
}

void EditorView::syncToDocument() {
    if (!doc_) return;
    if (loadingFromDoc_) return;
    doc_->setText(toPlainText());
}

bool EditorView::canInsertFromMimeData(const QMimeData* source) const {
    return source->hasImage() || QPlainTextEdit::canInsertFromMimeData(source);
}

void EditorView::insertFromMimeData(const QMimeData* source) {
    if (source->hasImage()) {
        QImage img = qvariant_cast<QImage>(source->imageData());
        if (!img.isNull()) {
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            if (!img.save(&buf, "PNG")) {
                qWarning("EditorView::insertFromMimeData: PNG encode failed; falling back to plain text");
                QPlainTextEdit::insertFromMimeData(source);
                return;
            }
            QFileInfo fi;
            QString ref;
            if (doc_) {
                fi.setFile(doc_->path());
            }
            QString docDir = fi.exists() ? fi.absolutePath()
                                         : QDir::currentPath();
            QString base = fi.exists() ? fi.completeBaseName() : QStringLiteral("untitled");
            ref = ImageHandler::handle(bytes, docDir, base);
            insertPlainText(QStringLiteral("![image](%1)").arg(ref));
            return;
        }
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void EditorView::applyEditorPalette(const mdreader::theme::EditorPalette& palette) {
    // Phase 12: push the palette through every paint-affecting channel:
    //   1. stylesheet on the QPlainTextEdit + its viewport — covers the
    //      text area background, foreground, selection highlight, and
    //      caret color in one shot;
    //   2. QPalette on the gutter widget (lineNumberArea_) so its auto-
    //      fillRect in paintLineNumbers() picks up the new colors (we also
    //      cache the colors explicitly below, but QPalette keeps it
    //      consistent if a stylesheet path is ever added that paints the
    //      gutter from CSS);
    //   3. cached colors for paintLineNumbers(), which paints imperatively
    //      via QPainter and so doesn't honor stylesheets;
    //   4. force a repaint on the gutter so the color change is visible
    //      immediately, not on the next paint event.
    setStyleSheet(QStringLiteral(
        "QPlainTextEdit{background:%1;color:%2;}"
        "QPlainTextEdit QWidget{color:%2;}"
        "QPlainTextEdit::viewport{background:%1;color:%2;}"
        "QPlainTextEdit::viewport QWidget{background:transparent;}"
        "QPlainTextEdit{selection-background-color:%3;selection-color:%4;}"
        "QPlainTextEdit QWidget{caret-color:%5;}"
        ).arg(palette.background.name(),
              palette.foreground.name(),
              palette.selectionBg.name(),
              palette.selectionFg.name(),
              palette.caret.name()));

    QPalette gutterPal = lineNumberArea_->palette();
    gutterPal.setColor(QPalette::Window,   palette.lineNumberBg);
    gutterPal.setColor(QPalette::WindowText, palette.lineNumberFg);
    gutterPal.setColor(QPalette::Base,     palette.lineNumberBg);
    gutterPal.setColor(QPalette::Text,     palette.lineNumberFg);
    lineNumberArea_->setPalette(gutterPal);
    // Auto-fill from palette background is only true if we set this
    // explicitly; otherwise the widget inherits the parent surface.
    lineNumberArea_->setAutoFillBackground(true);

    lineNumberBgCached_ = palette.lineNumberBg;
    lineNumberFgCached_ = palette.lineNumberFg;

    // Trigger an immediate repaint so the user sees the color change on
    // the very next frame instead of waiting for the next resize / scroll.
    lineNumberArea_->update();
    viewport()->update();
}
