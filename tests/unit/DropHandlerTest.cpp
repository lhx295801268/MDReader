#include <QtTest>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QPoint>
#include <QUrl>
#include <QList>
#include "app/DropHandler.h"

// Phase 8: drop-handler helper is a header-only pure function so we
// can test URL → path filtering without spinning up the MainWindow /
// QtWebEngine stack. Cases that matter:
//   - remote URLs (http://) are skipped
//   - non-existent files are skipped (dangling symlinks)
//   - non-markdown extensions are skipped
//   - directories are skipped
//   - markdown suffix matching is case-insensitive
//   - order is preserved
//   - canAccept stays cheap (no filesystem calls)
class DropHandlerTest : public QObject {
    Q_OBJECT
private slots:
    void canAccept_rejects_null_mime();
    void canAccept_rejects_remote_only();
    void canAccept_accepts_any_local_file();
    void extract_skips_remote_urls();
    void extract_skips_non_markdown_suffix();
    void extract_is_case_insensitive();
    void extract_skips_directories();
    void extract_skips_missing_files();
    void extract_preserves_order();
    void extract_returns_empty_for_empty_mime();
    void known_markdown_suffixes();
};

void DropHandlerTest::canAccept_rejects_null_mime() {
    QCOMPARE(mdreader::drop::canAccept(nullptr), false);
}

void DropHandlerTest::canAccept_rejects_remote_only() {
    auto* mime = new QMimeData;
    mime->setUrls({ QUrl("https://example.com/readme.md") });
    QCOMPARE(mdreader::drop::canAccept(mime), false);
    delete mime;
}

void DropHandlerTest::canAccept_accepts_any_local_file() {
    auto* mime = new QMimeData;
    // Even a non-markdown local file should make canAccept() return true
    // — we filter by suffix only at open time so dropping a folder of
    // mixed files doesn't get silently dropped.
    mime->setUrls({ QUrl::fromLocalFile("/tmp/foo.txt") });
    QCOMPARE(mdreader::drop::canAccept(mime), true);
    delete mime;
}

void DropHandlerTest::extract_skips_remote_urls() {
    auto* mime = new QMimeData;
    mime->setUrls({
        QUrl("https://example.com/readme.md"),
        QUrl::fromLocalFile("/tmp/exists.md"),
    });
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths.size(), 1);
    QCOMPARE(paths.first(), QString("/tmp/exists.md"));
    delete mime;
}

void DropHandlerTest::extract_skips_non_markdown_suffix() {
    auto* mime = new QMimeData;
    mime->setUrls({
        QUrl::fromLocalFile("/tmp/foo.txt"),
        QUrl::fromLocalFile("/tmp/bar.png"),
    });
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths.size(), 0);
    delete mime;
}

void DropHandlerTest::extract_is_case_insensitive() {
    // Create real temp files so QFileInfo::exists() returns true.
    const QString upper = QDir::tempPath() + "/drop_upper.MD";
    const QString mixed = QDir::tempPath() + "/drop_mixed.Markdown";
    {
        QFile f1(upper);  f1.open(QIODevice::WriteOnly);  f1.write("# x"); f1.close();
        QFile f2(mixed);  f2.open(QIODevice::WriteOnly);  f2.write("# y"); f2.close();
    }
    auto* mime = new QMimeData;
    mime->setUrls({ QUrl::fromLocalFile(upper), QUrl::fromLocalFile(mixed) });
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths.size(), 2);
    QVERIFY(paths.contains(upper));
    QVERIFY(paths.contains(mixed));
    QFile::remove(upper);
    QFile::remove(mixed);
    delete mime;
}

void DropHandlerTest::extract_skips_directories() {
    const QString dir = QDir::tempPath();   // exists & is a directory
    auto* mime = new QMimeData;
    mime->setUrls({ QUrl::fromLocalFile(dir) });
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths.size(), 0);
    delete mime;
}

void DropHandlerTest::extract_skips_missing_files() {
    auto* mime = new QMimeData;
    mime->setUrls({ QUrl::fromLocalFile("/tmp/definitely_does_not_exist.md") });
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths.size(), 0);
    delete mime;
}

void DropHandlerTest::extract_preserves_order() {
    const QString a = QDir::tempPath() + "/drop_a.md";
    const QString b = QDir::tempPath() + "/drop_b.md";
    const QString c = QDir::tempPath() + "/drop_c.md";
    for (const QString& p : {a, b, c}) {
        QFile f(p); f.open(QIODevice::WriteOnly); f.write("# x"); f.close();
    }
    auto* mime = new QMimeData;
    mime->setUrls({ QUrl::fromLocalFile(c), QUrl::fromLocalFile(a), QUrl::fromLocalFile(b) });
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths, QStringList({c, a, b}));
    QFile::remove(a); QFile::remove(b); QFile::remove(c);
    delete mime;
}

void DropHandlerTest::extract_returns_empty_for_empty_mime() {
    auto* mime = new QMimeData;
    QStringList paths = mdreader::drop::extractLocalMarkdownPaths(mime);
    QCOMPARE(paths.size(), 0);
    delete mime;
}

void DropHandlerTest::known_markdown_suffixes() {
    for (const QString& s : {"md", "markdown", "mdown", "mkd", "mkdn", "mdtxt",
                              "MD", "MARKDOWN", "Mkd"}) {
        QVERIFY2(mdreader::drop::isMarkdownSuffix(s),
                 qPrintable(QString("expected %1 to be recognized").arg(s)));
    }
    for (const QString& s : {"txt", "html", "docx", "", "pdf"}) {
        QVERIFY2(!mdreader::drop::isMarkdownSuffix(s),
                 qPrintable(QString("expected %1 to be rejected").arg(s)));
    }
}

QTEST_MAIN(DropHandlerTest)
#include "DropHandlerTest.moc"