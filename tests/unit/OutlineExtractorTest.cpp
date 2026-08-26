#include <QtTest>
#include "services/OutlineExtractor.h"

class OutlineExtractorTest : public QObject {
    Q_OBJECT
private slots:
    void extracts_h1_h2_h3_in_order();
    void ignores_code_fences();
};

void OutlineExtractorTest::extracts_h1_h2_h3_in_order() {
    QString md = "# Top\n\n## Sub\n\ntext\n\n### SubSub";
    auto entries = OutlineExtractor::extract(md);
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].text, QStringLiteral("Top"));
    QCOMPARE(entries[0].level, 1);
    QCOMPARE(entries[1].level, 2);
    QCOMPARE(entries[2].level, 3);
}

void OutlineExtractorTest::ignores_code_fences() {
    QString md = "```\n# fake heading\n```\n# real heading\n";
    auto entries = OutlineExtractor::extract(md);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].text, QStringLiteral("real heading"));
}

QTEST_MAIN(OutlineExtractorTest)
#include "OutlineExtractorTest.moc"
