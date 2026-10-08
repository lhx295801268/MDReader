#include <QtTest>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariant>
#include <QWebEnginePage>

#include "services/MarkdownRenderer.h"
#include "ui/PreviewView.h"

// Integration: the only layer that can prove a Mermaid diagram actually gets
// drawn. Parsing, script injection and DOM plumbing all look correct from a
// unit test, but the diagram only exists once Chromium has run mermaid against
// a real DOM — so these assertions read the live DOM back out of the page.
//
// The failure mode this guards is the one that motivated the feature: a
// ```mermaid block that renders as a plain <pre> of source. Asserting on
// `.node` (rather than just "an <svg> exists") is deliberate — mermaid emits
// an <svg> even for some error paths, so the node count is what proves a
// flowchart was really laid out.
//
// On QPA platform: QTEST_MAIN constructs a QApplication, and QtWebEngine needs
// a GUI platform. The ctest ENVIRONMENT property sets QT_QPA_PLATFORM=offscreen
// so this runs headless. The budget is generous (30s) because each case parses
// the 3.5 MB mermaid bundle from scratch.
class MermaidRenderTest : public QObject {
    Q_OBJECT
private slots:
    void renders_flowchart_to_svg_with_nodes();
    void broken_diagram_reports_error_and_keeps_rest_of_page();
    void diagram_free_document_does_not_load_mermaid();
    void export_html_is_self_contained();
    void reopened_export_still_shows_diagrams();
    void uppercase_fence_still_renders();
    void dark_theme_changes_the_diagram();
    void real_world_document_renders_every_diagram();
};

namespace {

// Qt 6 made runJavaScript callback-only. Spin the event loop until the value
// arrives so callers can assert on it directly.
QVariant evalJs(QWebEnginePage* page, const QString& js, int timeoutMs = 10000) {
    QVariant out;
    bool done = false;
    page->runJavaScript(js, [&out, &done](const QVariant& v) {
        out = v;
        done = true;
    });
    QElapsedTimer timer;
    timer.start();
    while (!done && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return out;
}

// Polls a JS expression returning a number until it goes positive. Mermaid
// renders asynchronously, so there is no signal to wait on — the DOM is the
// only completion signal available.
int waitForCount(QWebEnginePage* page, const QString& js, int timeoutMs = 30000) {
    QElapsedTimer timer;
    timer.start();
    int last = 0;
    while (timer.elapsed() < timeoutMs) {
        last = evalJs(page, js).toInt();
        if (last > 0) return last;
        QTest::qWait(100);
    }
    return last;
}

const char* kFlowchartMarkdown =
    "# 流程\n"
    "\n"
    "正文段落\n"
    "\n"
    "```mermaid\n"
    "flowchart TD\n"
    "    A[启动] --> B{判断}\n"
    "    B -- 是 --> C[结束]\n"
    "    B -- 否 --> A\n"
    "```\n";

// Verbatim from the FLOWCHART.md that motivated this feature: an unquoted
// parenthesis inside a node label. Mermaid reads '(' as the rounded-node
// delimiter and the diagram fails to parse.
const char* kBrokenDiagramMarkdown =
    "# 标题\n"
    "\n"
    "正文段落\n"
    "\n"
    "```mermaid\n"
    "flowchart TD\n"
    "    D3[查询 Object / Signal<br/>(已注册的 key)] --> E[结束]\n"
    "```\n";

// Builds a live preview for `markdown` and waits for the page to finish
// loading. Waiting on loadFinished (rather than a fixed sleep) matters: the
// vendor scripts in <head> are classic blocking scripts, so the parser has not
// even reached <body> while they download — document.body is null until then.
// Mermaid's own rendering continues after this returns; the poll helpers
// absorb that.
void loadPreview(PreviewView* pv, const QString& markdown, const QString& theme) {
    MarkdownRenderer renderer;
    pv->resize(1000, 700);
    pv->show();
    QTest::qWait(50);

    QSignalSpy loadSpy(pv->page(), &QWebEnginePage::loadFinished);
    pv->setMarkdownHtml(renderer.render(markdown, theme));
    loadSpy.wait(30000);
}

}  // namespace

void MermaidRenderTest::renders_flowchart_to_svg_with_nodes() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, QString::fromUtf8(kFlowchartMarkdown), QStringLiteral("github"));

    // The fenced block must have been converted off the <pre><code> path...
    QCOMPARE(waitForCount(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid').length")), 1);

    // ...and mermaid must have laid out real nodes, not just emitted an <svg>.
    const int nodes = waitForCount(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg .node').length"));
    QVERIFY2(nodes >= 3, qPrintable(QStringLiteral("expected at least 3 flowchart nodes, got %1")
                                        .arg(nodes)));

    // No error box, and highlight.js never saw the diagram source.
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid-error').length")).toInt(), 0);
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('code.language-mermaid').length")).toInt(), 0);
}

void MermaidRenderTest::broken_diagram_reports_error_and_keeps_rest_of_page() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, QString::fromUtf8(kBrokenDiagramMarkdown), QStringLiteral("github"));

    // The bad diagram surfaces a visible error instead of vanishing.
    QCOMPARE(waitForCount(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid-error').length")), 1);

    // The error box must name the failure and still show the source, so the
    // document author can see what to fix.
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid-error-src').length")).toInt(), 1);
    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "document.querySelector('.mdr-mermaid-error-head').textContent")).toString()
        .contains(QStringLiteral("失败")));

    // A broken diagram must not take the document down with it.
    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "document.body.innerText.includes('正文段落')")).toBool());
    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "document.querySelector('h1') !== null")).toBool());
}

void MermaidRenderTest::diagram_free_document_does_not_load_mermaid() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, QStringLiteral("# 只有文字\n\n没有图表\n"), QStringLiteral("github"));

    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "document.body.innerText.includes('只有文字')")).toBool());
    // The 3.5 MB bundle is only worth its parse when a diagram is present.
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "typeof window.MDReaderMermaid")).toString(), QStringLiteral("undefined"));
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('script[src*=\"mermaid\"]').length")).toInt(), 0);
}

void MermaidRenderTest::export_html_is_self_contained() {
    QVERIFY(QApplication::instance());

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/out.html";

    PreviewView pv;
    loadPreview(&pv, QString::fromUtf8(kFlowchartMarkdown), QStringLiteral("github"));
    QVERIFY(waitForCount(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg .node').length")) >= 3);

    // Mirror the production path: MainWindow captures the live DOM, then
    // PreviewView::exportHtml makes the qrc: references standalone.
    QString captured;
    pv.page()->toHtml([&captured](const QString& h) { captured = h; });
    QTRY_VERIFY_WITH_TIMEOUT(!captured.isEmpty(), 5000);
    QVERIFY(captured.contains(QStringLiteral("qrc:/vendor/")));

    pv.exportHtml(path, captured);

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString exported = QString::fromUtf8(f.readAll());
    f.close();

    // The whole point: no <script> is left pointing at a URL only the app can
    // resolve.
    //
    // Scoped to script tags on purpose. MathJax injects @font-face rules whose
    // url("qrc:/vendor/mathjax/output/chtml/fonts/...") still appears here —
    // those woff files were never vendored, in the app or the export, so math
    // already falls back to system fonts today. That is a pre-existing gap and
    // not something inlining a <script> could fix.
    static const QRegularExpression kQrcScriptTag(
        QStringLiteral(R"(<script\s+src=['"]qrc:)"));
    QVERIFY2(!kQrcScriptTag.match(exported).hasMatch(),
             "exported HTML still has a <script src='qrc:...'>");

    // ...and the bundles really are embedded, not dropped.
    //
    // These markers must be strings that appear ONLY in the bundle and not in
    // the bootstrap script that the page also carries. Plain "hljs" and
    // "MathJax" do not qualify — bootstrap itself says window.hljs and
    // window.MathJax, so asserting on those would pass with zero inlining.
    QVERIFY2(exported.contains(QStringLiteral("__esbuild_esm_mermaid_nm")),
             "mermaid bundle was not inlined");
    QVERIFY2(exported.contains(QStringLiteral("BSD-3-Clause")),
             "highlight.js bundle was not inlined");
    QVERIFY2(exported.contains(QStringLiteral("MathJax_Main-Regular")),
             "MathJax bundle was not inlined");
    QVERIFY(exported.size() > 1000000);
}

// The export test above only inspects the written bytes, which is why it
// passed while every exported diagram was in fact broken: the page captured by
// toHtml() carries the *already rendered* DOM, so re-opening it runs the
// bootstrap a second time over divs that now hold SVG instead of source.
// Asserting on the bytes cannot see that — only loading the file back can.
void MermaidRenderTest::reopened_export_still_shows_diagrams() {
    QVERIFY(QApplication::instance());

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/exported.html";

    {
        PreviewView pv;
        loadPreview(&pv, QString::fromUtf8(kFlowchartMarkdown), QStringLiteral("github"));
        QVERIFY(waitForCount(pv.page(), QStringLiteral(
            "document.querySelectorAll('.mdr-mermaid svg .node').length")) >= 3);

        QString captured;
        pv.page()->toHtml([&captured](const QString& h) { captured = h; });
        QTRY_VERIFY_WITH_TIMEOUT(!captured.isEmpty(), 5000);
        pv.exportHtml(path, captured);
    }

    // Load the exported file back the way a browser would: from disk, with no
    // application resources in reach.
    PreviewView reopened;
    reopened.resize(1000, 700);
    reopened.show();
    QTest::qWait(50);
    QSignalSpy loadSpy(reopened.page(), &QWebEnginePage::loadFinished);
    reopened.setUrl(QUrl::fromLocalFile(path));
    QVERIFY(loadSpy.wait(30000));

    // Give the re-run bootstrap time to do its damage (or not).
    QTest::qWait(4000);

    const int svgs = evalJs(reopened.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg').length")).toInt();
    const int errors = evalJs(reopened.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid-error').length")).toInt();
    const QString errText = evalJs(reopened.page(), QStringLiteral(
        "(document.querySelector('.mdr-mermaid-error-msg') || {}).textContent || ''")).toString();

    qInfo("  reopened export: svg=%d errors=%d", svgs, errors);
    QVERIFY2(errors == 0, qPrintable(QStringLiteral(
        "reopening the export replaced the diagram with an error box: %1").arg(errText)));
    QVERIFY2(svgs >= 1, "reopening the export lost the diagram");
}

// cmark passes the fence info string through verbatim, so ```Mermaid becomes
// class="language-Mermaid". Every lookup on the way to a rendered diagram has
// to normalise case, or the block is silently left as plain code.
void MermaidRenderTest::uppercase_fence_still_renders() {
    QVERIFY(QApplication::instance());

    const QString markdown = QStringLiteral(
        "# t\n\n"
        "```Mermaid\nflowchart TD\n    A[一] --> B[二]\n```\n\n"
        "```MERMAID\nflowchart LR\n    C[三] --> D[四]\n```\n");
    PreviewView pv;
    loadPreview(&pv, markdown, QStringLiteral("github"));

    QCOMPARE(waitForCount(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid').length")), 2);
    QCOMPARE(waitForCount(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg').length")), 2);
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('pre > code[class*=language-]').length")).toInt(), 0);
}

// The <body data-mermaid-theme> attribute carries our vocabulary (light/dark)
// while mermaid.initialize() wants mermaid's (default/dark). That mapping lives
// in JavaScript and would fail silently: an unrecognised theme name makes
// mermaid fall back to `default`, so a preview in a dark theme would quietly
// render a light diagram.
//
// Comparing the two renders catches exactly that. If the mapping broke, both
// themes would resolve to mermaid's fallback and the SVGs would be identical.
void MermaidRenderTest::dark_theme_changes_the_diagram() {
    QVERIFY(QApplication::instance());

    const QString md = QString::fromUtf8(kFlowchartMarkdown);

    PreviewView light;
    loadPreview(&light, md, QStringLiteral("github"));
    QVERIFY(waitForCount(light.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg').length")) >= 1);
    const QString lightSvg = evalJs(light.page(), QStringLiteral(
        "document.querySelector('.mdr-mermaid svg').outerHTML")).toString();

    PreviewView dark;
    loadPreview(&dark, md, QStringLiteral("dracula"));
    QVERIFY(waitForCount(dark.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg').length")) >= 1);
    const QString darkSvg = evalJs(dark.page(), QStringLiteral(
        "document.querySelector('.mdr-mermaid svg').outerHTML")).toString();

    QVERIFY(!lightSvg.isEmpty());
    QVERIFY(!darkSvg.isEmpty());
    QVERIFY2(lightSvg != darkSvg,
             "light and dark themes produced identical diagrams — the theme "
             "value is probably not being mapped onto a mermaid theme name");
    // A mermaid diagram carries its palette in an injected <style> block, so
    // the dark one should be reaching for light text.
    QVERIFY2(darkSvg.contains(QStringLiteral("#ccc")) || darkSvg.contains(QStringLiteral("#fff"))
                 || darkSvg.contains(QStringLiteral("fill:#f")),
             "the dark render does not look like mermaid's dark theme");
}

// Opt-in manual check against a document from the real world — the fixtures
// above are small enough to reason about, but the diagrams that motivated this
// feature live in a 30-node flowchart in somebody else's repository, and that
// is where surprising Mermaid syntax turns up.
//
//   MDREADER_MERMAID_DOC=/path/to/FLOWCHART.md \
//     QT_QPA_PLATFORM=offscreen ./bin/MermaidRenderTest real_world_document_renders_every_diagram
//
// The invariant asserted is the one this feature actually broke on first
// attempt: no diagram may end up silently blank. Every converted block must
// either carry a rendered <svg> or a visible error box — never neither.
void MermaidRenderTest::real_world_document_renders_every_diagram() {
    const QString path = qEnvironmentVariable("MDREADER_MERMAID_DOC");
    if (path.isEmpty()) {
        QSKIP("set MDREADER_MERMAID_DOC to a markdown file to run this check");
    }
    QFile src(path);
    QVERIFY2(src.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("cannot open %1").arg(path)));
    const QString markdown = QString::fromUtf8(src.readAll());
    src.close();

    // Ground truth comes from the markdown, not from the page. Counting hosts
    // would only see blocks that convert() already reached — a fence the
    // pipeline never noticed (wrong case, missing script injection) would be
    // invisible to the count and the check would pass on a half-rendered page.
    static const QRegularExpression kFence(
        QStringLiteral(R"(^```\s*mermaid\s*$)"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    int fences = 0;
    for (auto it = kFence.globalMatch(markdown); it.hasNext(); it.next()) ++fences;
    QVERIFY2(fences > 0, "document contains no ```mermaid fences");

    PreviewView pv;
    const QString theme = qEnvironmentVariable("MDREADER_MERMAID_THEME",
                                               QStringLiteral("github"));
    loadPreview(&pv, markdown, theme);

    const QString drawnJs = QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg').length");
    const QString errorsJs = QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid-error').length");
    // A fence that never became a host is still sitting in a <pre><code>.
    const QString leftoverJs = QStringLiteral(
        "Array.prototype.filter.call(document.querySelectorAll('pre > code'),"
        "  function (c) {"
        "    var m = /\\blanguage-([A-Za-z0-9_-]+)/.exec(c.getAttribute('class') || '');"
        "    return !!m && m[1].toLowerCase() === 'mermaid'; }).length");

    int drawn = 0, errors = 0, leftover = 0;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 60000) {
        drawn = evalJs(pv.page(), drawnJs).toInt();
        errors = evalJs(pv.page(), errorsJs).toInt();
        leftover = evalJs(pv.page(), leftoverJs).toInt();
        // Settled once every fence is accounted for. Leftovers are already
        // final after convert(); only drawing is still in flight.
        if (drawn + errors + leftover >= fences) break;
        QTest::qWait(250);
    }

    const QString firstErr = evalJs(pv.page(), QStringLiteral(
        "(document.querySelector('.mdr-mermaid-error-msg') || {}).textContent || ''")).toString();

    qInfo("document: %s", qUtf8Printable(path));
    qInfo("  fences in source: %d", fences);
    qInfo("  rendered: %d   error boxes: %d   left as code: %d",
          drawn, errors, leftover);
    if (!firstErr.isEmpty()) {
        qInfo("  first error: %s", qUtf8Printable(firstErr.section(QLatin1Char('\n'), 0, 0)));
    }

    QVERIFY2(leftover == 0, "a mermaid fence was never converted — it is still a code block");
    // The contract: every fence is accounted for by a diagram or an error box,
    // never by nothing at all.
    QCOMPARE(drawn + errors, fences);
}

QTEST_MAIN(MermaidRenderTest)
#include "MermaidRenderTest.moc"
