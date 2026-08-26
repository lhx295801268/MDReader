#include <QtTest>
#include <QApplication>
#include "services/MarkdownRenderer.h"

// Integration: MarkdownRenderer.render(...) must inject the chosen theme's
// CSS into the wrapped HTML, and switching theme must change the output.
// Uses unique color codes from each theme CSS as fingerprints:
//   - github.css body { color: #24292e; ... }
//   - dracula.css body { background: #282a36; ... }
// A literal substring search for the theme name ("github"/"dracula") would
// be ideal but those tokens don't appear in the CSS itself.
class ThemeSwitchTest : public QObject {
    Q_OBJECT
private slots:
    void different_themes_produce_different_html();
};

void ThemeSwitchTest::different_themes_produce_different_html() {
    QVERIFY(QApplication::instance());  // QTEST_MAIN provides it.

    MarkdownRenderer r;
    QString lightHtml = r.render("# Hi", "github");
    QString darkHtml  = r.render("# Hi", "dracula");

    // Each theme's unique CSS fingerprint must be embedded in its output.
    QVERIFY2(lightHtml.contains("#24292e"),
             "github theme CSS (#24292e body color) not injected");
    QVERIFY2(darkHtml.contains("#282a36"),
             "dracula theme CSS (#282a36 body background) not injected");

    // The cross-theme marker must NOT leak: github output should not contain
    // dracula's signature color, and vice versa.
    QVERIFY2(!lightHtml.contains("#282a36"),
             "light HTML unexpectedly contains dracula background");
    QVERIFY2(!darkHtml.contains("#24292e"),
             "dark HTML unexpectedly contains github body color");

    // Switching theme must change the rendered HTML.
    QVERIFY(lightHtml != darkHtml);
}

QTEST_MAIN(ThemeSwitchTest)
#include "ThemeSwitchTest.moc"