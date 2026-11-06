#include <QtTest>
#include "services/MarkdownRenderer.h"

// The zoom/pan viewer is what makes an oversized diagram or a high-resolution
// screenshot readable: the content is squeezed into the column, so the user
// needs a way to blow it up and pan around it.
//
// Unlike Mermaid, the viewer is injected on EVERY page — a document can hold
// a huge screenshot and no diagram at all. These tests pin that decision, and
// pin that the viewer's glue runs after the content that needs it exists.
//
// Note: unit tests do not register the qrc resources (see
// tests/integration/CMakeLists.txt for the ones that do), so the theme CSS
// comes back empty here. That is fine — the assertions are about script tags
// and their order, none of which depend on resource contents.
class ViewerInjectionTest : public QObject {
    Q_OBJECT
private slots:
    void injects_viewer_in_every_document();
    void viewer_precedes_the_bootstrap();
    void mermaid_stays_conditional();
    void bootstrap_enables_zoom_after_render();
};

void ViewerInjectionTest::injects_viewer_in_every_document() {
    MarkdownRenderer r;

    // A document with nothing but text still gets the viewer: the picture
    // that needs zooming may be the only "diagram" in the file.
    const QString plain = r.render(QStringLiteral("# 标题\n\n正文 *内容*\n"),
                                   QStringLiteral("github"));
    QVERIFY(plain.contains(QStringLiteral("viewer/viewer-overlay.js")));
    QVERIFY(plain.contains(QStringLiteral("viewer/preview-zoom.js")));

    // And so does one with a diagram — the viewer serves both.
    const QString withDiagram = r.render(
        QStringLiteral("```mermaid\nflowchart TD\n  A --> B\n```"),
        QStringLiteral("github"));
    QVERIFY(withDiagram.contains(QStringLiteral("viewer/viewer-overlay.js")));
    QVERIFY(withDiagram.contains(QStringLiteral("viewer/preview-zoom.js")));
}

void ViewerInjectionTest::viewer_precedes_the_bootstrap() {
    // The overlay has to exist before the bootstrap can hand it anything, and
    // the bootstrap must come after both viewer files so the page's own
    // DOMContentLoaded handler runs last.
    MarkdownRenderer r;
    const QString html = r.render(QStringLiteral("# t\n"), QStringLiteral("github"));

    const int overlay = html.indexOf(QStringLiteral("viewer-overlay.js"));
    const int zoom = html.indexOf(QStringLiteral("preview-zoom.js"));
    const int bootstrap = html.indexOf(QStringLiteral("DOMContentLoaded"));

    QVERIFY(overlay >= 0);
    QVERIFY(zoom >= 0);
    QVERIFY(bootstrap >= 0);
    QVERIFY(overlay < zoom);
    QVERIFY(zoom < bootstrap);
}

void ViewerInjectionTest::mermaid_stays_conditional() {
    // The viewer is cheap (tens of KB); the mermaid bundle is 3.5 MB and is
    // still only worth loading when a diagram is actually present.
    MarkdownRenderer r;
    const QString html = r.render(QStringLiteral("# 标题\n\n正文\n"),
                                  QStringLiteral("github"));

    QVERIFY(html.contains(QStringLiteral("viewer/viewer-overlay.js")));
    QVERIFY(!html.contains(QStringLiteral("mermaid.min.js")));
    QVERIFY(!html.contains(QStringLiteral("mermaid-init.js")));
}

void ViewerInjectionTest::bootstrap_enables_zoom_after_render() {
    // MathJax and Mermaid insert nodes after DOMContentLoaded, so the zoom
    // pass has to run after them — otherwise a late-arriving image would
    // never get its click handler.
    MarkdownRenderer r;
    const QString html = r.render(QStringLiteral("# t\n"), QStringLiteral("github"));

    const int zoomCall = html.indexOf(QStringLiteral("zoom.enhance()"));
    QVERIFY(zoomCall >= 0);
    // ...and it must come after the render call in the same handler.
    const int render = html.indexOf(QStringLiteral("diagrams.render()"));
    QVERIFY(render >= 0);
    QVERIFY(render < zoomCall);
}

QTEST_MAIN(ViewerInjectionTest)
#include "ViewerInjectionTest.moc"
