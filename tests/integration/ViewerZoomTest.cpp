#include <QtTest>
#include <QApplication>
#include <QBuffer>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariant>
#include <QWebEnginePage>

#include "services/MarkdownRenderer.h"
#include "ui/PreviewView.h"

// Integration: the zoom/pan viewer, driven against the real preview page.
//
// Unit tests can prove the viewer's scripts are injected; only a real
// Chromium can prove that clicking an image actually opens the overlay, that
// the zoom buttons change the scale, and that a drag moves the content.
//
// The fixture image is a 320x200 PNG inlined as a data: URI. cmark-gfm
// explicitly allows data:image/{png,gif,jpeg,webp} through its dangerous-URL
// filter (src/scanners.re), so the URI survives into the page intact — which
// also means the test needs no file on disk.
//
// On QPA platform: QTEST_MAIN constructs a QApplication, and QtWebEngine needs
// a GUI platform. The ctest ENVIRONMENT property sets QT_QPA_PLATFORM=offscreen
// so this runs headless.
class ViewerZoomTest : public QObject {
    Q_OBJECT
private slots:
    void image_is_marked_zoomable();
    void clicking_image_opens_the_overlay();
    void zoom_controls_change_the_scale();
    void fit_and_actual_size();
    void dragging_pans_the_content();
    void escape_closes_the_overlay();
    void toolbar_offers_visible_controls();
    void mermaid_diagram_uses_the_same_viewer();
    void exported_page_keeps_the_viewer();
};

namespace {

// A 320x200 PNG, inlined as a data: URI.
//
// The PNG is generated at run time rather than pasted in as base64: a
// hand-copied blob is exactly the kind of thing that silently loses a
// character and turns the test into a mystery. cmark-gfm explicitly allows
// data:image/{png,gif,jpeg,webp} through its dangerous-URL filter
// (src/scanners.re), so the URI survives into the page intact — which also
// means the test needs no file on disk.
QString pngDataUri(int w = 320, int h = 200) {
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(QColor(70, 130, 200));
    QByteArray bytes;
    QBuffer buf(&bytes);
    if (!buf.open(QIODevice::WriteOnly)) return {};
    img.save(&buf, "PNG");
    buf.close();
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

QString markdownWithImage() {
    return QStringLiteral("# 图文混排\n\n![示意图](%1)\n\n正文\n")
        .arg(pngDataUri());
}

QString markdownWithDiagram() {
    return QStringLiteral(
        "# 流程\n\n"
        "```mermaid\n"
        "flowchart TD\n"
        "    A[启动] --> B{判断}\n"
        "    B -- 是 --> C[结束]\n"
        "    B -- 否 --> A\n"
        "```\n");
}

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

// Polls a JS expression until it becomes truthy.
bool waitForJs(QWebEnginePage* page, const QString& js, int timeoutMs = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (evalJs(page, js).toBool()) return true;
        QTest::qWait(100);
    }
    return false;
}

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

void ViewerZoomTest::image_is_marked_zoomable() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));

    // The image has to be decoded before it can be measured, so wait for it.
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));

    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('img.mdr-zoomable').length")).toInt(), 1);
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelector('img').getAttribute('data-mdr-zoom')")).toString(),
        QStringLiteral("1"));
    // The viewer itself must be on the page — and present even though this
    // document has no diagram at all.
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "typeof window.MDReaderViewer")).toString(), QStringLiteral("object"));
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('script[src*=\"mermaid\"]').length")).toInt(), 0);
}

void ViewerZoomTest::clicking_image_opens_the_overlay() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));

    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.isOpen()")).toBool(), false);

    // A synthetic click is enough: the handler is a plain listener, and
    // dispatching trusted mouse events is not possible from here.
    evalJs(pv.page(), QStringLiteral("document.querySelector('img').click();"));

    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelector('.mdr-diagram-overlay').classList.contains('is-open')")).toBool(), true);

    // The overlay shows the image at its natural pixel size.
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getContentSize().w")).toInt(), 320);
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getContentSize().h")).toInt(), 200);
}

void ViewerZoomTest::zoom_controls_change_the_scale() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));
    evalJs(pv.page(), QStringLiteral("document.querySelector('img').click();"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));

    const double fit = evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getScale()")).toDouble();
    QVERIFY(fit > 0);

    evalJs(pv.page(), QStringLiteral("window.MDReaderViewer.zoomIn();"));
    const double zoomed = evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getScale()")).toDouble();
    QVERIFY2(zoomed > fit, qPrintable(QStringLiteral("zoomIn did not scale up (%1 -> %2)")
                                          .arg(fit).arg(zoomed)));

    evalJs(pv.page(), QStringLiteral("window.MDReaderViewer.zoomOut();"));
    const double back = evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getScale()")).toDouble();
    QVERIFY(qAbs(back - fit) < 1e-6);

    // The toolbar's zoom-in button must drive the same code path as the API.
    evalJs(pv.page(), QStringLiteral(
        "document.querySelector('.mdr-ov-toolbar .mdr-ov-btn').click();"));
    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getScale()")).toDouble() > fit);
}

void ViewerZoomTest::fit_and_actual_size() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));
    evalJs(pv.page(), QStringLiteral("document.querySelector('img').click();"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));

    // 1:1 means one image pixel per screen pixel.
    evalJs(pv.page(), QStringLiteral("window.MDReaderViewer.actualSize();"));
    QVERIFY(qAbs(evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getScale()")).toDouble() - 1.0) < 1e-6);

    // Fit never magnifies past 1:1 — a blown-up bitmap is just a blurry one.
    evalJs(pv.page(), QStringLiteral("window.MDReaderViewer.fit();"));
    const double fit = evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getScale()")).toDouble();
    QVERIFY(fit > 0);
    QVERIFY(fit <= 1.0);
}

void ViewerZoomTest::dragging_pans_the_content() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));
    evalJs(pv.page(), QStringLiteral("document.querySelector('img').click();"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));

    const double before = evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getOffset().tx")).toDouble();

    evalJs(pv.page(), QStringLiteral(R"JS(
      (function () {
        var ov = document.querySelector('.mdr-diagram-overlay');
        ov.dispatchEvent(new MouseEvent('mousedown',
          {clientX: 100, clientY: 100, bubbles: true, button: 0}));
        window.dispatchEvent(new MouseEvent('mousemove',
          {clientX: 160, clientY: 130, bubbles: true, button: 0}));
        window.dispatchEvent(new MouseEvent('mouseup',
          {clientX: 160, clientY: 130, bubbles: true, button: 0}));
      })();
    )JS"));

    const double after = evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getOffset().tx")).toDouble();
    QVERIFY2(qAbs(after - before - 60) < 1.0,
             qPrintable(QStringLiteral("drag did not pan by 60px (%1 -> %2)")
                            .arg(before).arg(after)));
}

void ViewerZoomTest::escape_closes_the_overlay() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));
    evalJs(pv.page(), QStringLiteral("document.querySelector('img').click();"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));

    evalJs(pv.page(), QStringLiteral(
        "document.dispatchEvent(new KeyboardEvent('keydown',"
        " {key: 'Escape', bubbles: true, cancelable: true}));"));

    QVERIFY(waitForJs(pv.page(), QStringLiteral("!window.MDReaderViewer.isOpen()")));
}

void ViewerZoomTest::toolbar_offers_visible_controls() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));
    evalJs(pv.page(), QStringLiteral("document.querySelector('img').click();"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));

    // Zoom in / zoom out / fit / 1:1 / close, plus the scale readout.
    QCOMPARE(evalJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-ov-toolbar .mdr-ov-btn').length")).toInt(), 5);
    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "document.querySelector('.mdr-ov-scale').textContent")).toString()
        .contains(QStringLiteral("%")));
}

void ViewerZoomTest::mermaid_diagram_uses_the_same_viewer() {
    QVERIFY(QApplication::instance());

    PreviewView pv;
    loadPreview(&pv, markdownWithDiagram(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "document.querySelectorAll('.mdr-mermaid svg .node').length >= 3")));

    // SVGElement has no .click() method (that is HTMLElement's), so the
    // event has to be dispatched by hand.
    evalJs(pv.page(), QStringLiteral(
        "document.querySelector('.mdr-mermaid svg')"
        ".dispatchEvent(new MouseEvent('click', {bubbles: true}));"));

    QVERIFY(waitForJs(pv.page(), QStringLiteral("window.MDReaderViewer.isOpen()")));
    // The diagram's own size drives the fit, not the image path.
    QVERIFY(evalJs(pv.page(), QStringLiteral(
        "window.MDReaderViewer.getContentSize().w")).toInt() > 0);
}

void ViewerZoomTest::exported_page_keeps_the_viewer() {
    QVERIFY(QApplication::instance());

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/exported.html";

    PreviewView pv;
    loadPreview(&pv, markdownWithImage(), QStringLiteral("github"));
    QVERIFY(waitForJs(pv.page(), QStringLiteral(
        "(function(){var i=document.querySelector('img');return !!i && i.complete && i.naturalWidth>0;})()")));

    QString captured;
    pv.page()->toHtml([&captured](const QString& h) { captured = h; });
    QTRY_VERIFY_WITH_TIMEOUT(!captured.isEmpty(), 5000);

    pv.exportHtml(path, captured);

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString exported = QString::fromUtf8(f.readAll());
    f.close();

    // The viewer has to be inlined, otherwise an exported document loses the
    // zoom/pan feature entirely.
    QVERIFY2(exported.contains(QStringLiteral("MDReaderViewer")),
             "the zoom/pan viewer was not inlined into the export");
    QVERIFY2(exported.contains(QStringLiteral("mdr-ov-toolbar")),
             "the viewer toolbar markup was not inlined into the export");
}

QTEST_MAIN(ViewerZoomTest)
#include "ViewerZoomTest.moc"
