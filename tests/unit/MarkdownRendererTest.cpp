#include <QtTest>
#include <QList>
#include <QString>
#include <QThread>
#include <atomic>
#include "services/MarkdownRenderer.h"

class MarkdownRendererTest : public QObject {
    Q_OBJECT
private slots:
    void renders_plain_paragraph();
    void renders_table();
    void renders_fenced_code_with_language_class();
    void preserves_math_block();
    void thread_safe_concurrent_calls();
};

void MarkdownRendererTest::renders_plain_paragraph() {
    MarkdownRenderer r;
    QString html = r.render("Hello *world*", QStringLiteral("github"));
    QVERIFY(html.contains("<p>"));
    QVERIFY(html.contains("<em>world</em>"));
}

void MarkdownRendererTest::renders_table() {
    QString md = "| a | b |\n|---|---|\n| 1 | 2 |";
    MarkdownRenderer r;
    QString html = r.render(md, QStringLiteral("github"));
    QVERIFY(html.contains("<table>"));
    QVERIFY(html.contains("<td>1</td>"));
}

void MarkdownRendererTest::renders_fenced_code_with_language_class() {
    QString md = "```python\nprint('hi')\n```";
    MarkdownRenderer r;
    QString html = r.render(md, QStringLiteral("github"));
    QVERIFY(html.contains("language-python"));
}

void MarkdownRendererTest::preserves_math_block() {
    QString md = "$$E=mc^2$$";
    MarkdownRenderer r;
    QString html = r.render(md, QStringLiteral("github"));
    QVERIFY(html.contains("$$E=mc^2$$") ||
            html.contains("\\[E=mc^2\\]"));
}

void MarkdownRendererTest::thread_safe_concurrent_calls() {
    MarkdownRenderer r;
    std::atomic<int> errors{0};
    QList<QThread*> threads;
    for (int i = 0; i < 8; ++i) {
        auto* t = QThread::create([&] {
            for (int j = 0; j < 50; ++j) {
                QString h = r.render(QStringLiteral("a *b* c"), QStringLiteral("github"));
                if (!h.contains("<em>b</em>")) ++errors;
            }
        });
        threads << t;
        t->start();
    }
    for (auto* t : threads) t->wait();
    QCOMPARE(errors.load(), 0);
}

QTEST_MAIN(MarkdownRendererTest)
#include "MarkdownRendererTest.moc"