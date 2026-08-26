#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include "documents/Document.h"
#include "ui/EditorView.h"
#include "ui/PreviewView.h"
#include "ui/DocumentTab.h"
#include "services/RenderCoordinator.h"

// Integration: end-to-end editor -> preview rendering through the real
// RenderCoordinator pipeline. Wires up a Document (text="# Hi"), constructs
// a DocumentTab (which builds EditorView + PreviewView), binds the
// RenderCoordinator to the tab's preview, asks for a render, and then
// asserts the rendered preview DOM contains "Hi" — i.e. the markdown made
// it from the Document into the rendered HTML visible in the QWebEngineView.
//
// requestRender in Live mode (default) starts the 250ms debounce timer;
// we rely on the 3000ms QTRY_VERIFY_WITH_TIMEOUT budget to absorb it.
// force=true is a no-op in Live mode — it only bypasses Manual-mode
// short-circuit (see RenderCoordinator::requestRender).
//
// On QPA platform: QTEST_MAIN constructs a QApplication that needs a GUI
// platform. The ctest ENVIRONMENT property on this test sets
// QT_QPA_PLATFORM=offscreen so the test runs headless in CI.
//
// Qt 6 API note: QWebEnginePage::toPlainText() is callback-only — the
// no-arg overload was removed. We follow the same pattern used in
// HtmlExportTest for toHtml(): capture the result into a QString via the
// callback lambda, then QTRY_VERIFY_WITH_TIMEOUT on the captured value.
class EditToPreviewTest : public QObject {
    Q_OBJECT
private slots:
    void edit_in_editor_appears_in_preview();
};

void EditToPreviewTest::edit_in_editor_appears_in_preview() {
    QVERIFY(QApplication::instance());  // QTEST_MAIN provides it.

    auto doc = std::make_shared<Document>(QString(), "# Hi");
    DocumentTab tab(doc);
    tab.show();
    QTest::qWait(100);  // let window event loop process before we trigger a render

    // Use the 3-arg bind() overload (Task 20) so the worker thread can
    // bounce the freshly-computed outline entries + word stats back to the
    // tab via tab->onContentUpdated — mirrors the production wiring in
    // MainWindow. The 2-arg legacy overload would also satisfy this test
    // (we only assert on the preview HTML here), but we use 3-arg to stay
    // consistent with how the app actually wires up.
    RenderCoordinator rc;
    rc.bind(doc->path(), tab.preview(), &tab);
    rc.requestRender(doc->path(), doc->text(), "github", /*force=*/true);

    // Wait for the page to finish loading the freshly-rendered HTML, then
    // capture the rendered plain text via the Qt 6 callback overload and
    // spin the event loop until it arrives.
    QSignalSpy loadSpy(tab.preview()->page(), &QWebEnginePage::loadFinished);
    QVERIFY(loadSpy.isValid());
    QVERIFY(loadSpy.wait(3000));

    QString capturedText;
    tab.preview()->page()->toPlainText([&capturedText](const QString& t) { capturedText = t; });
    QTRY_VERIFY_WITH_TIMEOUT(capturedText.contains(QStringLiteral("Hi")), 3000);
}

QTEST_MAIN(EditToPreviewTest)
#include "EditToPreviewTest.moc"
