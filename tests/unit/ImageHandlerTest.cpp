#include <QtTest>
#include <QTemporaryDir>
#include "services/ImageHandler.h"

class ImageHandlerTest : public QObject {
    Q_OBJECT
private slots:
    void same_bytes_same_path();
    void different_bytes_different_files();
    void creates_assets_dir_if_missing();
};

void ImageHandlerTest::same_bytes_same_path() {
    QTemporaryDir tmp;
    QByteArray bytes("\x89PNG\r\n\x1a\n fake");
    QString a = ImageHandler::handle(bytes, tmp.path(), "doc");
    QString b = ImageHandler::handle(bytes, tmp.path(), "doc");
    QCOMPARE(a, b);
}

void ImageHandlerTest::different_bytes_different_files() {
    QTemporaryDir tmp;
    QString a = ImageHandler::handle(QByteArray("aaaa"), tmp.path(), "doc");
    QString b = ImageHandler::handle(QByteArray("bbbb"), tmp.path(), "doc");
    QVERIFY(a != b);
}

void ImageHandlerTest::creates_assets_dir_if_missing() {
    QTemporaryDir tmp;
    QString ref = ImageHandler::handle(QByteArray("data"), tmp.path(), "doc");
    QDir d(tmp.path() + "/doc.assets");
    QVERIFY(d.exists());
    QVERIFY(QFile::exists(tmp.path() + "/" + ref));
}

QTEST_MAIN(ImageHandlerTest)
#include "ImageHandlerTest.moc"
