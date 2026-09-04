#pragma once
// Phase 12: in-editor find bar (Ctrl+F / Ctrl+G / Esc).
//
// A small horizontal strip that floats over the active tab's editor and
// offers inline search across the document text. The strip contains:
//
//   [ Find:   <QLineEdit>      ]  [ ^ ]  [ v ]  [ 1 / 7 ]  [ x ]
//
// Behavior:
//   - Ctrl+F on MainWindow (with focus on the editor) calls showFor(editor).
//     showFor() captures the editor pointer and the current selection as
//     the seed for the line edit, so a quick "search for what I just
//     selected" works without retyping.
//   - Typing in the line edit triggers an incremental find forward from
//     the current cursor position. The match counter updates on every
//     keystroke.
//   - Up/Down arrow buttons move to the previous / next match.
//   - Enter / Shift+Enter mirror the Down/Up buttons.
//   - Esc hides the bar and returns focus to the editor. The
//     findBarClosed signal lets MainWindow restore focus to the right
//     child (some users expect focus on the editor, others on the tab
//     itself — MainWindow decides).
//
// We deliberately reuse QPlainTextEdit::find() instead of implementing
// search from scratch: it already handles cursor navigation, selection
// highlighting, and the wrap-to-top behavior. We just count matches with
// a temporary QTextDocument::find() loop so the "1 / 7" label stays
// accurate as the user types.
#include <QWidget>
class QLineEdit;
class QToolButton;
class QLabel;
class QPlainTextEdit;
class QShowEvent;
class QHideEvent;

class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QWidget* parent = nullptr);

    // Bind to a new editor (e.g. user switched tabs) and reveal the bar
    // with the editor's current selection as the search seed. If the
    // selection is empty the line edit is cleared.
    void showFor(QPlainTextEdit* editor);

    // True if the bar is currently visible. MainWindow uses this to know
    // whether to forward the Ctrl+F shortcut to the bar (already-open
    // bar should re-focus the input rather than dismiss + re-open).
    bool isVisibleBar() const;

    // Programmatic close — also emits findBarClosed(). Public so MainWindow
    // can dismiss the bar when switching tabs, and so unit tests can
    // verify the signal without simulating a button click.
    void closeBar();

signals:
    // Emitted when the bar hides itself (Esc or the close button). The
    // host typically moves keyboard focus back to the editor here.
    void findBarClosed();

private slots:
    void onQueryChanged(const QString& text);
    void findNext();
    void findPrev();

private:
    void updateMatchCount();
    QPlainTextEdit* boundEditor() const;

    QLineEdit*   query_     = nullptr;
    QToolButton* prevBtn_   = nullptr;
    QToolButton* nextBtn_   = nullptr;
    QToolButton* closeBtn_  = nullptr;
    QLabel*      status_    = nullptr;
    QPlainTextEdit* editor_ = nullptr;  // not owned; tracked only
};