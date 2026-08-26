#include <QtTest>
#include "documents/Document.h"

class DocumentTest : public QObject {
    Q_OBJECT
private slots:
    void new_document_is_empty_and_not_dirty();
    void setText_marks_dirty();
    void markSaved_clears_dirty();
    void text_returns_copy();
};

void DocumentTest::new_document_is_empty_and_not_dirty() {
    Document d;
    QCOMPARE(d.text(), QString());
    QCOMPARE(d.dirty(), false);
    QCOMPARE(d.path(), QString());
}

void DocumentTest::setText_marks_dirty() {
    Document d;
    d.setText("hello");
    QCOMPARE(d.text(), QStringLiteral("hello"));
    QCOMPARE(d.dirty(), true);
}

void DocumentTest::markSaved_clears_dirty() {
    Document d;
    d.setText("hello");
    d.markSaved();
    QCOMPARE(d.dirty(), false);
}

void DocumentTest::text_returns_copy() {
    Document d;
    d.setText("hello");
    QString a = d.text();
    QString b = d.text();
    a.append('!');
    QCOMPARE(d.text(), QStringLiteral("hello"));
    QVERIFY(a != b);
}

QTEST_MAIN(DocumentTest)
#include "DocumentTest.moc"
