#include <QtTest>
#include "services/WordCounter.h"

class WordCounterTest : public QObject {
    Q_OBJECT
private slots:
    void empty();
    void english();
    void mixed_cjk_latin();
};

void WordCounterTest::empty() {
    WordCounter::Stats s = WordCounter::count("");
    QCOMPARE(s.words, 0); QCOMPARE(s.chars, 0);
    QCOMPARE(s.paragraphs, 0); QCOMPARE(s.headings, 0);
}

void WordCounterTest::english() {
    auto s = WordCounter::count("# Hello world\n\nThis *is* a test.");
    QCOMPARE(s.headings, 1);
    QCOMPARE(s.paragraphs, 2);
    QVERIFY(s.words >= 6);
    QVERIFY(s.chars > 0);
}

void WordCounterTest::mixed_cjk_latin() {
    auto s = WordCounter::count("你好 world");
    QVERIFY(s.words >= 2);
    QVERIFY(s.chars >= 7);
}

QTEST_MAIN(WordCounterTest)
#include "WordCounterTest.moc"
