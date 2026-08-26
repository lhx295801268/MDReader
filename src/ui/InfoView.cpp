#include "ui/InfoView.h"

InfoView::InfoView(QWidget* parent) : QWidget(parent) {
    auto* form = new QFormLayout(this);
    words_ = new QLabel("0"); chars_ = new QLabel("0");
    paras_ = new QLabel("0"); heads_ = new QLabel("0");
    form->addRow("Words", words_);
    form->addRow("Chars", chars_);
    form->addRow("Paragraphs", paras_);
    form->addRow("Headings", heads_);
}

void InfoView::setStats(int w, int c, int p, int h) {
    words_->setNum(w); chars_->setNum(c); paras_->setNum(p); heads_->setNum(h);
}
