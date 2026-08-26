#include <QtTest>

class SanityTest : public QObject {
    Q_OBJECT
private slots:
    void math_works();
};

void SanityTest::math_works() {
    QCOMPARE(1 + 1, 2);
}

QTEST_MAIN(SanityTest)
#include "SanityTest.moc"
