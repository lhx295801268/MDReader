#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "ui/PreviewView.h"

// Integration: a real QWebEngineView loads HTML, we read its rendered DOM
// via toHtml(), and write that string to disk via PreviewView::exportHtml.
// Then we assert the file exists and is non-empty.
//
// Note on QPA platform: QTEST_MAIN constructs a QApplication, which needs a
// GUI platform. The ctest ENVIRONMENT property on this test sets
// QT_QPA_PLATFORM=offscreen so the test runs headless in CI. Run the binary
// directly only on a box with a display (or export QT_QPA_PLATFORM=offscreen
// yourself).
class HtmlExportTest : public QObject {
    Q_OBJECT
private slots:
    void exports_html_to_file();
};

void HtmlExportTest::exports_html_to_file() {
    QVERIFY(QApplication::instance());  // QTEST_MAIN provides it.

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/out.html";

    PreviewView pv;
    pv.show();
    QCoreApplication::processEvents();

    pv.page()->setHtml("<p>hello</p>");

    // Wait for the page to finish loading before we ask for HTML. Qt 6
    // removed the no-arg overloads of toHtml()/toPlainText() — both are
    // async callbacks. We wait for loadFinished so the DOM is settled, then
    // capture the HTML via a callback and spin the event loop until it
    // arrives.
    QSignalSpy loadSpy(pv.page(), &QWebEnginePage::loadFinished);
    QVERIFY(loadSpy.isValid());
    QVERIFY(loadSpy.wait(2000));

    QString capturedHtml;
    pv.page()->toHtml([&capturedHtml](const QString& h) { capturedHtml = h; });
    QTRY_VERIFY_WITH_TIMEOUT(!capturedHtml.isEmpty(), 2000);
    QVERIFY(capturedHtml.contains(QStringLiteral("hello")));

    pv.exportHtml(path, capturedHtml);

    QVERIFY(QFile::exists(path));
    QVERIFY(QFile(path).size() > 0);
}

QTEST_MAIN(HtmlExportTest)
#include "HtmlExportTest.moc"