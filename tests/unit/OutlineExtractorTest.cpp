#include <QtTest>
#include "services/OutlineExtractor.h"

class OutlineExtractorTest : public QObject {
    Q_OBJECT
private slots:
    void extracts_h1_h2_h3_in_order();
    void ignores_code_fences();
    void preserves_atx_trailing_hash();
    void indented_code_block_does_not_open_fence();
    void heading_after_closed_fence_with_blanks();
    void empty_input_yields_empty_list();
    void line_numbers_are_one_indexed();
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

void OutlineExtractorTest::preserves_atx_trailing_hash() {
    // Intentional behavior: trailing `#` chars are kept as part of the text.
    QString md = "# Title #";
    auto entries = OutlineExtractor::extract(md);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].text, QStringLiteral("Title #"));
    QCOMPARE(entries[0].level, 1);
}

void OutlineExtractorTest::indented_code_block_does_not_open_fence() {
    // CommonMark: 4+ spaces of indent starts an indented code block, NOT a
    // fence. A `# heading` line after it must still be picked up.
    QString md = "    # indented (code)\n# heading\n";
    auto entries = OutlineExtractor::extract(md);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].text, QStringLiteral("heading"));
}

void OutlineExtractorTest::heading_after_closed_fence_with_blanks() {
    QString md = "```\n# fake\n```\n\n\n# real\n";
    auto entries = OutlineExtractor::extract(md);
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].text, QStringLiteral("real"));
}

void OutlineExtractorTest::empty_input_yields_empty_list() {
    auto entries = OutlineExtractor::extract(QString());
    QCOMPARE(entries.size(), 0);
}

void OutlineExtractorTest::line_numbers_are_one_indexed() {
    // Third heading sits on line 3 of the input.
    QString md = "# One\n# Two\n# Three";
    auto entries = OutlineExtractor::extract(md);
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].lineNumber, 1);
    QCOMPARE(entries[1].lineNumber, 2);
    QCOMPARE(entries[2].lineNumber, 3);
}

QTEST_MAIN(OutlineExtractorTest)
#include "OutlineExtractorTest.moc"
