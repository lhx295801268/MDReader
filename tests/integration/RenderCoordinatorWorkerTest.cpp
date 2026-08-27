#include <QtTest>
#include <QApplication>
#include <QDateTime>
#include "documents/Document.h"
#include "ui/PreviewView.h"
#include "ui/DocumentTab.h"
#include "services/RenderCoordinator.h"

// Integration: RenderCoordinator::requestRender must NOT block the main
// thread while the worker computes the render. The plan asserts the
// enqueue should take < 50ms; a 1MB+ markdown would block the main
// thread 200-500ms if rendering happened synchronously here.
//
// We only time the requestRender ENQUEUE (the function body just stores
// the inputs and restarts the 250ms debounce timer) — we do NOT spin the
// event loop to wait for the worker thread to finish. So this test only
// asserts that requestRender returns promptly; the actual worker-offload
// behavior is covered by EditToPreviewTest's full render round-trip.
//
// Qt 6 API note: plan spec's `if (!QApplication::instance()) { ... }
// qApp->argc()` block is dead code — QCoreApplication* has no argc()/argv()
// members and Qt 6 cannot reach main's argc/argv from a test slot. The
// ctest ENVIRONMENT property sets QT_QPA_PLATFORM=offscreen so QTEST_MAIN
// can construct a GUI QApplication headless. Match peer tests
// (EditToPreviewTest, ThemeSwitchTest, ExternalReloadTest): a single
// QVERIFY(QApplication::instance()).
class RenderCoordinatorWorkerTest : public QObject {
    Q_OBJECT
private slots:
    void main_thread_responds_quickly_while_worker_renders();
};

void RenderCoordinatorWorkerTest::main_thread_responds_quickly_while_worker_renders() {
    QVERIFY(QApplication::instance());  // QTEST_MAIN provides it.

    // 1MB+ 的 markdown,正常同步渲染会卡主线程 200-500ms。
    QString big = "# 大标题\n\n" + QString(1000, 'a') + QString("\n\n");
    for (int i = 0; i < 1000; ++i) big += QStringLiteral("段落%1 **bold** *em* `code`\n").arg(i);

    auto doc = std::make_shared<Document>(QString(), big);
    DocumentTab tab(doc);
    tab.show();
    QTest::qWait(100);  // let window event loop process before we trigger a render

    // 2-arg legacy bind(): the test only times the enqueue; we don't need
    // the worker → tab bounce path (covered by EditToPreviewTest).
    RenderCoordinator rc;
    rc.bind(doc->path(), tab.preview());

    auto t0 = QDateTime::currentMSecsSinceEpoch();
    rc.requestRender(doc->path(), doc->text(), "github", /*force*/true);
    auto t1 = QDateTime::currentMSecsSinceEpoch();
    // requestRender 的入队应该 < 50ms(主线程不阻塞)。
    QVERIFY2(t1 - t0 < 50,
             qPrintable(QString("requestRender took %1ms, expect < 50ms").arg(t1 - t0)));
}

QTEST_MAIN(RenderCoordinatorWorkerTest)
#include "RenderCoordinatorWorkerTest.moc"
