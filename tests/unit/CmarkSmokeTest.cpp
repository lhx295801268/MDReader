#include <QtTest>
#include <cstring>
#include <cstdlib>
#include <cmark-gfm.h>

class CmarkSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void parses_hello_world();
};

void CmarkSmokeTest::parses_hello_world() {
    const char* md = "hello world";
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    QVERIFY(parser != nullptr);
    cmark_parser_feed(parser, md, strlen(md));
    cmark_node* doc = cmark_parser_finish(parser);
    QVERIFY(doc != nullptr);
    char* html = cmark_render_html(doc, CMARK_OPT_DEFAULT, nullptr);
    QVERIFY(html != nullptr);
    QVERIFY(QString::fromUtf8(html).contains("hello world"));
    std::free(html);
    cmark_node_free(doc);
    cmark_parser_free(parser);
}

QTEST_MAIN(CmarkSmokeTest)
#include "CmarkSmokeTest.moc"
