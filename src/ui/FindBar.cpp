#include "ui/FindBar.h"
#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QShortcut>
#include <QKeySequence>

FindBar::FindBar(QWidget* parent) : QWidget(parent) {
    // Float-style strip; no border / shadow because MainWindow docks us
    // directly above the editor so a heavy chrome would clash with the
    // editor's own look.
    setAutoFillBackground(true);
    QPalette bg = palette();
    bg.setColor(QPalette::Window, QColor(245, 245, 245));
    setPalette(bg);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto* caption = new QLabel(tr("Find:"), this);
    layout->addWidget(caption);

    query_ = new QLineEdit(this);
    query_->setPlaceholderText(tr("Type to search…"));
    query_->setClearButtonEnabled(true);
    query_->setMinimumWidth(220);
    layout->addWidget(query_, 1);

    prevBtn_ = new QToolButton(this);
    prevBtn_->setText(QStringLiteral("▲"));  // ▲
    prevBtn_->setToolTip(tr("Previous match (Shift+Enter)"));
    layout->addWidget(prevBtn_);

    nextBtn_ = new QToolButton(this);
    nextBtn_->setText(QStringLiteral("▼"));  // ▼
    nextBtn_->setToolTip(tr("Next match (Enter)"));
    layout->addWidget(nextBtn_);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("findBarStatus"));
    status_->setMinimumWidth(64);
    status_->setAlignment(Qt::AlignCenter);
    layout->addWidget(status_);

    closeBtn_ = new QToolButton(this);
    closeBtn_->setText(QStringLiteral("✕"));  // ✕
    closeBtn_->setToolTip(tr("Close (Esc)"));
    layout->addWidget(closeBtn_);

    connect(query_, &QLineEdit::textChanged, this, &FindBar::onQueryChanged);
    connect(query_, &QLineEdit::returnPressed, this, &FindBar::findNext);
    connect(prevBtn_, &QToolButton::clicked, this, &FindBar::findPrev);
    connect(nextBtn_, &QToolButton::clicked, this, &FindBar::findNext);
    connect(closeBtn_, &QToolButton::clicked, this, &FindBar::closeBar);

    // Esc inside the line edit dismisses the bar — most editors handle
    // Esc this way and users expect it. The shortcut is scoped to the
    // line edit's child tree so the rest of MainWindow keeps Esc for
    // whatever it wants to use it for.
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), query_);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &FindBar::closeBar);

    hide();
}

bool FindBar::isVisibleBar() const { return isVisible(); }

void FindBar::showFor(QPlainTextEdit* editor) {
    editor_ = editor;
    QString seed;
    if (editor_) {
        QTextCursor c = editor_->textCursor();
        if (c.hasSelection()) seed = c.selectedText();
    }
    query_->setText(seed);
    show();
    query_->setFocus();
    query_->selectAll();
    // Always run an initial find so the match counter shows a real number
    // even if the user opens the bar with empty input.
    updateMatchCount();
}

void FindBar::onQueryChanged(const QString& /*text*/) {
    auto* ed = boundEditor();
    if (!ed) return;
    // Move cursor to the start so find() advances from there — this gives
    // a predictable "find from top of file" behavior for every keystroke.
    // If the user wants search-from-cursor they can press Enter / the
    // Next button explicitly.
    QTextCursor c = ed->textCursor();
    c.clearSelection();
    c.movePosition(QTextCursor::Start);
    ed->setTextCursor(c);
    findNext();
}

void FindBar::findNext() {
    auto* ed = boundEditor();
    if (!ed) return;
    const QString needle = query_->text();
    if (needle.isEmpty()) {
        status_->clear();
        return;
    }
    // QPlainTextEdit::find returns true when it located the next match
    // and advanced+selected the cursor. Forward search is the default;
    // an explicit zero flags argument means "no case change, no wrap" —
    // we accept the default wrapping behavior (wraps to top once).
    bool found = ed->find(needle);
    if (!found) {
        status_->setText(tr("no match"));
    }
    updateMatchCount();
}

void FindBar::findPrev() {
    auto* ed = boundEditor();
    if (!ed) return;
    const QString needle = query_->text();
    if (needle.isEmpty()) {
        status_->clear();
        return;
    }
    bool found = ed->find(needle, QTextDocument::FindBackward);
    if (!found) {
        status_->setText(tr("no match"));
    }
    updateMatchCount();
}

void FindBar::closeBar() {
    hide();
    emit findBarClosed();
}

void FindBar::updateMatchCount() {
    auto* ed = boundEditor();
    if (!ed) { status_->clear(); return; }
    const QString needle = query_->text();
    if (needle.isEmpty()) { status_->clear(); return; }
    // Count matches by scanning the document with a fresh cursor each time.
    // We don't cache the count because (a) the doc can be edited while the
    // bar is open and (b) a re-scan on every keystroke is cheap for the
    // typical markdown sizes we target.
    QTextDocument* doc = ed->document();
    int count = 0;
    int current = 0;  // 1-indexed position of the active match
    QTextCursor probe(doc);
    QTextCursor active = ed->textCursor();
    int activePos = active.selectionStart();
    while (true) {
        probe = doc->find(needle, probe);
        if (probe.isNull()) break;
        ++count;
        if (probe.selectionStart() == activePos) current = count;
    }
    if (count == 0) {
        status_->setText(tr("no match"));
    } else {
        // If the active cursor isn't on any match (e.g. user pressed Up/Down
        // and the active selection moved), show "— / N" instead of guessing.
        if (current == 0) {
            status_->setText(QStringLiteral("/ %1").arg(count));
        } else {
            status_->setText(QStringLiteral("%1 / %2").arg(current).arg(count));
        }
    }
}

QPlainTextEdit* FindBar::boundEditor() const { return editor_; }