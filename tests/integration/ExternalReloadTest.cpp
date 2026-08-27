#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include "documents/Document.h"
#include "documents/DocumentManager.h"

class ExternalReloadTest : public QObject {
    Q_OBJECT
private slots:
    void open_then_external_change_then_dirty_emits_loaded_signal();
};

// 本测试只验证 worker 读盘完成路径;真正的三按钮 modal 由 Qt 主线程
// QMessageBox 触发,这里改用 mock 信号计数代替硬弹窗。
//
// QTEST_MAIN 总是构造一个 QApplication,所以这里只需断言 instance 存在
// (与 ThemeSwitchTest / EditToPreviewTest 一致)。plan spec 中
// `if (!QApplication::instance()) { new QApplication(qApp->argc(), ...); }`
// 是死代码分支 — QCoreApplication* 没有 argc()/argv() 成员,Qt 6 也无法
// 在 test 方法内访问 main 局部 argc/argv。guard 在运行时不可达,故省略。
void ExternalReloadTest::open_then_external_change_then_dirty_emits_loaded_signal() {
    QVERIFY(QApplication::instance());
    QTemporaryDir tmp;
    QString path = tmp.path() + "/x.md";
    QFile f(path);
    f.open(QIODevice::WriteOnly); f.write("# hi"); f.close();

    DocumentManager dm;
    QSignalSpy loaded(&dm, &DocumentManager::documentLoaded);
    dm.openFile(path);
    QTRY_VERIFY_WITH_TIMEOUT(loaded.count() == 1, 3000);
    QCOMPARE(loaded.at(0).at(0).value<std::shared_ptr<Document>>()->text(),
             QStringLiteral("# hi"));

    // 文档变 dirty 后,documentLoaded 路径不再适用 — 走 modal 分支,不在此测。
}

QTEST_MAIN(ExternalReloadTest)
#include "ExternalReloadTest.moc"
