#pragma once
#include <QWidget>
#include <QSplitter>
#include <memory>
#include "services/OutlineExtractor.h"
#include "services/WordCounter.h"

class Document;
class EditorView;
class PreviewView;
class OutlineView;
class InfoView;

/// A single document tab: triple-nested horizontal QSplitter that arranges
/// [Outline | Editor | Preview | Info] left-to-right.
class DocumentTab : public QWidget {
    Q_OBJECT
public:
    explicit DocumentTab(std::shared_ptr<Document> doc, QWidget* parent = nullptr);

    EditorView* editor() const;
    PreviewView* preview() const;
    OutlineView* outline() const;
    InfoView* info() const;

    QSplitter* splitterA() const { return splitterA_; }
    QSplitter* splitterB() const { return splitterB_; }
    QSplitter* splitterC() const { return splitterC_; }

public slots:
    // Task 20: invoked by RenderCoordinator on the main thread after each
    // successful render. Updates the outline tree and word/char/para/heading
    // counters. Pure UI update — no thread affinity concerns beyond the
    // caller being on the main thread.
    void onContentUpdated(const QList<OutlineExtractor::Entry>& entries,
                          const WordCounter::Stats& stats);

private:
    void buildUi();

    std::shared_ptr<Document> doc_;
    QSplitter* splitterA_ = nullptr;
    QSplitter* splitterB_ = nullptr;
    QSplitter* splitterC_ = nullptr;
    EditorView* editor_ = nullptr;
    PreviewView* preview_ = nullptr;
    OutlineView* outline_ = nullptr;
    InfoView* info_ = nullptr;
};