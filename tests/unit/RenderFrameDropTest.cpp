#include <QtTest>
#include <QSignalSpy>
#include "services/RenderCoordinator.h"
#include "services/WorkerThread.h"

class RenderFrameDropTest : public QObject {
    Q_OBJECT
private slots:
    void render_stale_html_is_dropped_when_newer_render_arrives();
    void render_emit_success_for_known_receiver();
    void render_real_pipeline_drops_stale_frame();
};

void RenderFrameDropTest::render_stale_html_is_dropped_when_newer_render_arrives() {
    RenderCoordinator coord;
    coord.bind(QStringLiteral("doc1"), nullptr);  // preview=nullptr is OK
    QSignalSpy spy(&coord, &RenderCoordinator::renderSucceeded);
    QVERIFY(spy.isValid());

    QString html = QStringLiteral("<p>x</p>");
    coord.simulateLateDelivery(QStringLiteral("doc1"), /*frameId*/ 1,
                               /*stale*/ true, html);
    coord.simulateLateDelivery(QStringLiteral("doc1"), /*frameId*/ 2,
                               /*stale*/ false, html);
    QCOMPARE(spy.count(), 1);
}

void RenderFrameDropTest::render_emit_success_for_known_receiver() {
    RenderCoordinator coord;
    QSignalSpy spy(&coord, &RenderCoordinator::renderSucceeded);
    coord.simulateLateDelivery(QStringLiteral("doc1"), 1, false,
                               QStringLiteral("<p>ok</p>"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("<p>ok</p>"));
}

void RenderFrameDropTest::render_real_pipeline_drops_stale_frame() {
    // 真实管线测试:绑定 doc,触发两次 requestRender 在 250ms 窗口内,
    // 确认只有最新的 frame 触发了 renderSucceeded。
    RenderCoordinator coord;
    coord.bind(QStringLiteral("doc1"), nullptr);
    QSignalSpy spy(&coord, &RenderCoordinator::renderSucceeded);

    coord.requestRender(QStringLiteral("doc1"), QStringLiteral("# A"), QStringLiteral("github"));
    // 立即再触发 → 帧序号前进,旧 timer 被 restart 取消。
    coord.requestRender(QStringLiteral("doc1"), QStringLiteral("# B"), QStringLiteral("github"));

    // 等待 timer (250ms) + worker 渲染 + bounce 回主线程。
    QVERIFY(spy.wait(2000));
    QCOMPARE(spy.count(), 1);
    // 最新的 markdown 是 "# B" → 渲染的 HTML 应包含 "B" 而非 "A"。
    const QString html = spy.at(0).at(0).toString();
    QVERIFY2(html.contains(QStringLiteral("B")), "expected newest frame's HTML");
}

QTEST_MAIN(RenderFrameDropTest)
#include "RenderFrameDropTest.moc"
