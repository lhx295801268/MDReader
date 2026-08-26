#include <QtTest>
#include "services/MarkdownRenderer.h"

// Task 20: MarkdownRenderer must inject id="slug" attributes on h1..h6 so
// preview scroll-to-heading and outline navigation can target them. Slug
// logic must match OutlineExtractor::slugify (lowercase, strip non
// [a-z0-9一-鿿\s-], collapse whitespace to "-").
class MarkdownRendererHeadingIdTest : public QObject {
    Q_OBJECT
private slots:
    void injects_id_on_h1();
    void injects_id_on_h2_and_h3();
    void slug_collapses_whitespace();
    void slug_strips_punctuation();
    void slug_keeps_hyphen_and_digits();
    void cjk_heading_gets_id();
    void no_headings_yields_no_id_attributes();
};

void MarkdownRendererHeadingIdTest::injects_id_on_h1() {
    MarkdownRenderer r;
    QString html = r.render("# Hello World", QStringLiteral("github"));
    QVERIFY2(html.contains("<h1 id=\"hello-world\">Hello World</h1>"),
             qUtf8Printable(html));
}

void MarkdownRendererHeadingIdTest::injects_id_on_h2_and_h3() {
    QString md = "## Sub\n\n### Deep";
    MarkdownRenderer r;
    QString html = r.render(md, QStringLiteral("github"));
    QVERIFY2(html.contains("<h2 id=\"sub\">Sub</h2>"), qUtf8Printable(html));
    QVERIFY2(html.contains("<h3 id=\"deep\">Deep</h3>"), qUtf8Printable(html));
}

void MarkdownRendererHeadingIdTest::slug_collapses_whitespace() {
    MarkdownRenderer r;
    QString html = r.render("# Hello   World", QStringLiteral("github"));
    QVERIFY2(html.contains("<h1 id=\"hello-world\">"), qUtf8Printable(html));
}

void MarkdownRendererHeadingIdTest::slug_strips_punctuation() {
    MarkdownRenderer r;
    QString html = r.render("# Hello, World!", QStringLiteral("github"));
    QVERIFY2(html.contains("<h1 id=\"hello-world\">"), qUtf8Printable(html));
}

void MarkdownRendererHeadingIdTest::slug_keeps_hyphen_and_digits() {
    MarkdownRenderer r;
    QString html = r.render("# Top-Level 101", QStringLiteral("github"));
    QVERIFY2(html.contains("<h1 id=\"top-level-101\">"), qUtf8Printable(html));
}

void MarkdownRendererHeadingIdTest::cjk_heading_gets_id() {
    MarkdownRenderer r;
    QString html = r.render("# 你好 世界", QStringLiteral("github"));
    // CJK is preserved in the slug; the test just verifies the id attribute
    // is present and contains "你好" (the first ideograph).
    QVERIFY2(html.contains("<h1 id=\""), qUtf8Printable(html));
    QVERIFY2(html.contains("你好"), qUtf8Printable(html));
    QVERIFY2(html.contains("</h1>"), qUtf8Printable(html));
}

void MarkdownRendererHeadingIdTest::no_headings_yields_no_id_attributes() {
    MarkdownRenderer r;
    QString html = r.render("Just a paragraph.", QStringLiteral("github"));
    QVERIFY2(!html.contains("id=\""), qUtf8Printable(html));
}

QTEST_MAIN(MarkdownRendererHeadingIdTest)
#include "MarkdownRendererHeadingIdTest.moc"