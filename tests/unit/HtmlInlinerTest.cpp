#include <QtTest>
#include "services/HtmlInliner.h"

// Export-support tests for the pure parts of inlineQrcScripts(). Unit test
// executables do not register the qrc resources (only integration targets do —
// see tests/integration/CMakeLists.txt), so the *positive* inlining path is
// covered end-to-end by integration_MermaidRenderTest, which has the real
// vendor bundles on hand.
//
// What is pinned here is the degradation contract, which is the part that
// decides whether a broken export silently loses a <script> tag:
//   - HTML with nothing to inline comes back byte-identical
//   - a qrc tag whose resource cannot be read is preserved verbatim
//   - non-qrc script sources are never touched
class HtmlInlinerTest : public QObject {
    Q_OBJECT
private slots:
    void leaves_html_without_qrc_scripts_untouched();
    void preserves_tag_when_resource_is_missing();
    void ignores_remote_script_sources();
    void preserves_surrounding_markup();
};

void HtmlInlinerTest::leaves_html_without_qrc_scripts_untouched() {
    const QString html = QStringLiteral(
        "<!doctype html><html><head><style>body{color:red}</style></head>"
        "<body><h1 id=\"a\">hi</h1><p>text</p></body></html>");
    QCOMPARE(mdreader::html::inlineQrcScripts(html), html);
}

void HtmlInlinerTest::preserves_tag_when_resource_is_missing() {
    // The tag must survive: an export that silently drops the script would be
    // strictly worse than today's behaviour, which at least keeps a tag that
    // works when the file is opened inside the app.
    const QString html = QStringLiteral(
        "<head><script src='qrc:/vendor/does/not/exist.js'></script></head>");
    QCOMPARE(mdreader::html::inlineQrcScripts(html), html);
}

void HtmlInlinerTest::ignores_remote_script_sources() {
    // Anything that is not a qrc: URL is somebody else's to load — rewriting it
    // would change semantics we do not own.
    const QString html = QStringLiteral(
        "<head><script src='https://example.com/x.js'></script>"
        "<script src='/relative/y.js'></script></head>");
    QCOMPARE(mdreader::html::inlineQrcScripts(html), html);
}

void HtmlInlinerTest::preserves_surrounding_markup() {
    // Several tags in one document: every one that cannot be inlined must come
    // back in place, in order, with the markup between them intact.
    const QString html = QStringLiteral(
        "<head>"
        "<style>h1{color:blue}</style>"
        "<script src='qrc:/vendor/a.js'></script>"
        "<script src=\"qrc:/vendor/b.js\"></script>"
        "</head>"
        "<body data-mermaid-theme='light'><h1 id=\"t\">标题</h1></body>");
    const QString out = mdreader::html::inlineQrcScripts(html);

    QCOMPARE(out, html);
    QVERIFY(out.indexOf(QStringLiteral("qrc:/vendor/a.js"))
            < out.indexOf(QStringLiteral("qrc:/vendor/b.js")));
    QVERIFY(out.contains(QStringLiteral("data-mermaid-theme='light'")));
    QVERIFY(out.contains(QStringLiteral("标题")));
}

QTEST_MAIN(HtmlInlinerTest)
#include "HtmlInlinerTest.moc"
