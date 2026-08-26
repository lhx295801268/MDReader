#include "ui/EditorView.h"
#include <QPainter>
#include <QMimeData>
#include <QImage>
#include <QBuffer>
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

void EditorView::paintEvent(QPaintEvent* e) {
    QPlainTextEdit::paintEvent(e);
    // 行号与缩进的连接点在 updateRequest 信号里 — 这里不再重复。
}

void EditorView::paintLineNumbers(QPaintEvent* e) {
    if (!showLineNumbers_) return;
    QPainter p(lineNumberArea_);
    p.fillRect(lineNumberArea_->rect(), QColor(245, 245, 245));
    QTextBlock block = firstVisibleBlock();
    int top = blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + blockBoundingRect(block).height();
    int current = block.blockNumber() + 1;
    QFont f = font(); f.setBold(false);
    p.setFont(f); p.setPen(QColor(150, 150, 150));
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
    blockSignals(true);
    setPlainText(t);
    blockSignals(false);
}

void EditorView::syncToDocument() {
    if (!doc_) return;
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
            img.save(&buf, "PNG");
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
