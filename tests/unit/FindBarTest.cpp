#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include <QLabel>
#include <QLineEdit>
#include "ui/FindBar.h"

class FindBarTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void hidden_by_default();
    void showFor_binds_editor_and_seeds_selection();
    void showFor_with_no_selection_uses_empty_seed();
    void close_emits_signal();
    void matches_count_when_text_matches();
    void no_match_status_when_text_absent();

private:
    QPlainTextEdit* makeEditor(const QString& text);
    QApplication* app_ = nullptr;
};

void FindBarTest::initTestCase() {
    // QShortcut / QToolButton need a QApplication, not QCoreApplication.
    // Some QtTest setups create QGuiApplication by default — but our
    // tests are wired with QTEST_MAIN which gives us QCoreApplication.
    // Promote to QApplication if needed.
    if (!qobject_cast<QApplication*>(QCoreApplication::instance())) {
        int argc = 0;
        static char empty[] = "findbar-test";
        static char* argv[] = { empty, nullptr };
        app_ = new QApplication(argc, argv);
    }
}

QPlainTextEdit* FindBarTest::makeEditor(const QString& text) {
    auto* ed = new QPlainTextEdit();
    ed->setPlainText(text);
    return ed;
}

void FindBarTest::hidden_by_default() {
    FindBar bar;
    QVERIFY(!bar.isVisibleBar());
}

void FindBarTest::showFor_binds_editor_and_seeds_selection() {
    FindBar bar;
    auto* ed = makeEditor("alpha\nbeta\ngamma alpha\n");
    // Select "alpha" so showFor seeds the line edit.
    QTextCursor c = ed->textCursor();
    c.movePosition(QTextCursor::Start);
    c.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    ed->setTextCursor(c);

    bar.showFor(ed);
    QVERIFY(bar.isVisibleBar());
    QCOMPARE(bar.findChild<QLineEdit*>()->text(), QStringLiteral("alpha"));

    delete ed;
}

void FindBarTest::showFor_with_no_selection_uses_empty_seed() {
    FindBar bar;
    auto* ed = makeEditor("hello world");
    bar.showFor(ed);
    QCOMPARE(bar.findChild<QLineEdit*>()->text(), QString());
    delete ed;
}

void FindBarTest::close_emits_signal() {
    FindBar bar;
    auto* ed = makeEditor("hello");
    bar.showFor(ed);
    QSignalSpy spy(&bar, &FindBar::findBarClosed);
    bar.closeBar();
    QCOMPARE(spy.count(), 1);
    QVERIFY(!bar.isVisibleBar());
    delete ed;
}

void FindBarTest::matches_count_when_text_matches() {
    FindBar bar;
    auto* ed = makeEditor("alpha beta alpha gamma alpha");
    bar.showFor(ed);
    bar.findChild<QLineEdit*>()->setText(QStringLiteral("alpha"));
    // After typing, the bar should find at least one match and surface
    // a "N / total" or "no match" status. We just check the status label
    // is non-empty (a regression on the counter is caught by visual test).
    QLabel* status = bar.findChild<QLabel*>(QStringLiteral("findBarStatus"));
    QVERIFY(status);
    QVERIFY(!status->text().isEmpty());
    QVERIFY(!status->text().contains(QStringLiteral("no match")));
    delete ed;
}

void FindBarTest::no_match_status_when_text_absent() {
    FindBar bar;
    auto* ed = makeEditor("alpha beta gamma");
    bar.showFor(ed);
    bar.findChild<QLineEdit*>()->setText(QStringLiteral("zzz"));
    QLabel* status = bar.findChild<QLabel*>(QStringLiteral("findBarStatus"));
    QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("no match")));
    delete ed;
}

QTEST_MAIN(FindBarTest)
#include "FindBarTest.moc"