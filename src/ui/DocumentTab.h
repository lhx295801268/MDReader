#pragma once
#include <QWidget>
#include <QSplitter>
#include <memory>

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