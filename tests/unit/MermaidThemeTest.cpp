#include <QtTest>
#include "services/MermaidTheme.h"

// The preview theme name is the single input that decides which of Mermaid's
// built-in themes a diagram gets. Getting this table wrong is not a crash — it
// is a white diagram on a black page, which is exactly the class of bug a unit
// test is cheap insurance against.
//
// Keep the dark list in lockstep with src/resources/themes/*.css: an entry here
// must be a stylesheet whose `body { background: ... }` is dark.
class MermaidThemeTest : public QObject {
    Q_OBJECT
private slots:
    void dark_themes_are_flagged_dark();
    void light_themes_are_not_dark();
    void unknown_theme_falls_back_to_light();
    void theme_attribute_matches_mermaid_vocabulary();
};

void MermaidThemeTest::dark_themes_are_flagged_dark() {
    for (const QString& t : {QStringLiteral("github-dark"), QStringLiteral("dracula"),
                             QStringLiteral("solarized-dark"), QStringLiteral("one-dark")}) {
        QVERIFY2(mdreader::mermaid::isDarkTheme(t), qPrintable(t));
    }
}

void MermaidThemeTest::light_themes_are_not_dark() {
    for (const QString& t : {QStringLiteral("github"), QStringLiteral("solarized-light")}) {
        QVERIFY2(!mdreader::mermaid::isDarkTheme(t), qPrintable(t));
    }
}

void MermaidThemeTest::unknown_theme_falls_back_to_light() {
    // An unrecognised name must still produce a usable page. Light is the safe
    // default: the bundled themes are mostly light and a light diagram on an
    // unknown background is at worst low-contrast, never invisible.
    for (const QString& t : {QString(), QStringLiteral("nope"), QStringLiteral("GITHUB-DARK"),
                             QStringLiteral("github_dark")}) {
        QVERIFY(!mdreader::mermaid::isDarkTheme(t));
        QCOMPARE(mdreader::mermaid::mermaidThemeFor(t), QStringLiteral("light"));
    }
}

void MermaidThemeTest::theme_attribute_matches_mermaid_vocabulary() {
    // The attribute value is written into <body data-mermaid-theme>, and
    // mermaid-init.js maps it onto a Mermaid theme name. The two vocabularies
    // are deliberately different, and the mapping is load-bearing: Mermaid has
    // no theme called "light" (its built-ins are default / forest / dark /
    // neutral / base), so the attribute value must never be handed to
    // mermaid.initialize() unchanged.
    for (const QString& t : {QStringLiteral("github"), QStringLiteral("solarized-light"),
                             QStringLiteral("unknown-theme")}) {
        QCOMPARE(mdreader::mermaid::mermaidThemeFor(t), QStringLiteral("light"));
    }
    for (const QString& t : {QStringLiteral("dracula"), QStringLiteral("one-dark")}) {
        QCOMPARE(mdreader::mermaid::mermaidThemeFor(t), QStringLiteral("dark"));
    }
}

QTEST_MAIN(MermaidThemeTest)
#include "MermaidThemeTest.moc"
