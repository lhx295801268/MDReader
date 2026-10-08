#include <QtTest>
#include "services/MarkdownRenderer.h"

// wrapHtml() decides whether the preview page pulls in Mermaid at all. That
// decision is a real trade-off, not a detail: mermaid.min.js is 3.5 MB, and
// live-render mode reloads the entire page on every debounced keystroke, so an
// unconditional <script src> would make typing in any diagram-free document pay
// a full JS parse it never uses.
//
// These tests pin both halves of the decision: the scripts appear when a
// diagram is present, and stay out when it is not.
//
// Note: unit tests do not register the qrc resources (see
// tests/integration/CMakeLists.txt for the ones that do), so theme CSS comes
// back empty here. That is fine — the assertions are about script tags and the
// body attribute, none of which depend on resource contents.
class MermaidInjectionTest : public QObject {
    Q_OBJECT
private slots:
    void injects_scripts_when_diagram_present();
    void omits_scripts_when_no_diagram();
    void omits_scripts_for_other_languages();
    void body_carries_theme_attribute();
    void diagram_source_survives_to_the_page();
    void bootstrap_orders_convert_before_render();
};

void MermaidInjectionTest::injects_scripts_when_diagram_present() {
    MarkdownRenderer r;
    const QString html = r.render(
        QStringLiteral("```mermaid\nflowchart TD\n  A[启动] --> B[结束]\n```"),
        QStringLiteral("github"));

    QVERIFY(html.contains(QStringLiteral("vendor/mermaid/mermaid.min.js")));
    QVERIFY(html.contains(QStringLiteral("vendor/mermaid/mermaid-init.js")));
    // The bundle must be present before the glue that drives it.
    QVERIFY(html.indexOf(QStringLiteral("mermaid.min.js"))
            < html.indexOf(QStringLiteral("mermaid-init.js")));
}

void MermaidInjectionTest::omits_scripts_when_no_diagram() {
    MarkdownRenderer r;
    const QString html = r.render(QStringLiteral("# 标题\n\n正文 *内容*\n"),
                                  QStringLiteral("github"));

    QVERIFY(!html.contains(QStringLiteral("mermaid.min.js")));
    QVERIFY(!html.contains(QStringLiteral("mermaid-init.js")));
    // ...and the assets every page needs are still there.
    QVERIFY(html.contains(QStringLiteral("vendor/highlight/highlight.min.js")));
    QVERIFY(html.contains(QStringLiteral("vendor/mathjax/tex-mml-chtml.js")));
}

void MermaidInjectionTest::omits_scripts_for_other_languages() {
    // A non-mermaid block that merely mentions the word must not trigger the
    // 3.5 MB load; only cmark's language-mermaid class counts.
    MarkdownRenderer r;
    const QString html = r.render(
        QStringLiteral("```python\n# mermaid is not used here\nprint('hi')\n```"),
        QStringLiteral("github"));

    QVERIFY(html.contains(QStringLiteral("language-python")));
    QVERIFY(!html.contains(QStringLiteral("mermaid.min.js")));
}

void MermaidInjectionTest::body_carries_theme_attribute() {
    MarkdownRenderer r;
    const QString md = QStringLiteral("```mermaid\nflowchart TD\n  A --> B\n```");

    const QString light = r.render(md, QStringLiteral("github"));
    QVERIFY(light.contains(QStringLiteral("data-mermaid-theme='light'")));

    const QString dark = r.render(md, QStringLiteral("dracula"));
    QVERIFY(dark.contains(QStringLiteral("data-mermaid-theme='dark'")));
}

void MermaidInjectionTest::diagram_source_survives_to_the_page() {
    // mermaid-init.js reads the diagram out of the DOM, so the fenced block has
    // to reach the page as a code element with the right language class.
    MarkdownRenderer r;
    const QString html = r.render(
        QStringLiteral("```mermaid\nflowchart TD\n  A[启动] --> B[结束]\n```"),
        QStringLiteral("github"));

    QVERIFY(html.contains(QStringLiteral("language-mermaid")));
    QVERIFY(html.contains(QStringLiteral("A[启动] --&gt; B[结束]"))
            || html.contains(QStringLiteral("A[启动] --> B[结束]")));
}

void MermaidInjectionTest::bootstrap_orders_convert_before_render() {
    // Convert must run before highlight.js so the diagram source is never
    // syntax-highlighted, and render must come after MathJax so MathJax never
    // walks into a freshly inserted SVG.
    MarkdownRenderer r;
    const QString html = r.render(
        QStringLiteral("```mermaid\nflowchart TD\n  A --> B\n```"),
        QStringLiteral("github"));

    const int convert = html.indexOf(QStringLiteral("diagrams.convert()"));
    const int mathjax = html.indexOf(QStringLiteral("MathJax.typesetPromise"));
    const int highlight = html.indexOf(QStringLiteral("hljs.highlightAll"));
    const int render = html.indexOf(QStringLiteral("diagrams.render()"));

    QVERIFY(convert >= 0);
    QVERIFY(mathjax >= 0);
    QVERIFY(highlight >= 0);
    QVERIFY(render >= 0);
    QVERIFY(convert < mathjax);
    QVERIFY(mathjax < render);
    QVERIFY(highlight < render);
}

QTEST_MAIN(MermaidInjectionTest)
#include "MermaidInjectionTest.moc"
