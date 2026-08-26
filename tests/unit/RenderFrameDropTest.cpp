#include <QtTest>
#include <QSignalSpy>
#include "services/RenderCoordinator.h"
#include "services/WorkerThread.h"

class RenderFrameDropTest : public QObject {
    Q_OBJECT
private slots:
    void render_stale_html_is_dropped_when_newer_render_arrives();
    void render_emit_success_for_known_receiver();
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

QTEST_MAIN(RenderFrameDropTest)
#include "RenderFrameDropTest.moc"
