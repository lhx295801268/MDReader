#include "ui/DocumentTab.h"
#include "documents/Document.h"
#include "ui/EditorView.h"
#include "ui/PreviewView.h"
#include "ui/OutlineView.h"
#include "ui/InfoView.h"

DocumentTab::DocumentTab(std::shared_ptr<Document> doc, QWidget* parent)
    : QWidget(parent), doc_(std::move(doc)) {
    buildUi();
}

void DocumentTab::buildUi() {
    editor_ = new EditorView(this);
    editor_->setDocument(doc_);
    preview_ = new PreviewView(this);
    outline_ = new OutlineView(this);
    info_ = new InfoView(this);

    splitterC_ = new QSplitter(Qt::Horizontal, this);
    splitterC_->addWidget(preview_);
    splitterC_->addWidget(info_);
    splitterC_->setStretchFactor(0, 1);
    splitterC_->setStretchFactor(1, 0);

    splitterB_ = new QSplitter(Qt::Horizontal, this);
    splitterB_->addWidget(editor_);
    splitterB_->addWidget(splitterC_);
    splitterB_->setStretchFactor(0, 1);
    splitterB_->setStretchFactor(1, 1);

    splitterA_ = new QSplitter(Qt::Horizontal, this);
    splitterA_->addWidget(outline_);
    splitterA_->addWidget(splitterB_);
    splitterA_->setStretchFactor(0, 0);
    splitterA_->setStretchFactor(1, 1);

    splitterA_->setSizes({180, 1000});
    splitterB_->setSizes({500, 500});
    splitterC_->setSizes({700, 220});

    info_->setMinimumWidth(150);
    outline_->setMinimumWidth(120);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitterA_);
}

EditorView* DocumentTab::editor() const { return editor_; }
PreviewView* DocumentTab::preview() const { return preview_; }
OutlineView* DocumentTab::outline() const { return outline_; }
InfoView* DocumentTab::info() const { return info_; }