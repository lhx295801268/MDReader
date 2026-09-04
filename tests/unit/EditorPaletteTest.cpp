#include <QtTest>
#include "app/EditorPalette.h"

class EditorPaletteTest : public QObject {
    Q_OBJECT
private slots:
    void known_themes_return_named_palettes();
    void unknown_theme_falls_back_to_light();
    void light_themes_have_light_background();
    void dark_themes_have_dark_background();
    void light_vs_dark_contrast();
    void selection_colors_contrast_with_background();
};

void EditorPaletteTest::known_themes_return_named_palettes() {
    // Every theme in ThemeMenu's available() list must produce a non-empty
    // palette with a valid (non-default-constructed) background color.
    // A default-constructed QColor is invalid — checking isValid() guards
    // against typos in the lookup table silently returning garbage.
    const QStringList themes = {"github", "github-dark", "dracula",
                                "solarized-light", "solarized-dark",
                                "one-dark"};
    for (const auto& t : themes) {
        auto p = mdreader::theme::paletteForTheme(t);
        QVERIFY2(p.background.isValid(), qPrintable(t));
        QVERIFY2(p.foreground.isValid(), qPrintable(t));
        QVERIFY2(p.lineNumberBg.isValid(), qPrintable(t));
        QVERIFY2(p.lineNumberFg.isValid(), qPrintable(t));
        QVERIFY2(p.selectionBg.isValid(), qPrintable(t));
        QVERIFY2(p.selectionFg.isValid(), qPrintable(t));
        QVERIFY2(p.caret.isValid(), qPrintable(t));
    }
}

void EditorPaletteTest::unknown_theme_falls_back_to_light() {
    // Empty / typo'd / random strings must not return a default-constructed
    // (invalid) palette. The contract is: fallback to github (light).
    auto p1 = mdreader::theme::paletteForTheme(QString());
    auto p2 = mdreader::theme::paletteForTheme(QStringLiteral("nonexistent"));
    auto pRef = mdreader::theme::paletteForTheme(QStringLiteral("github"));
    QCOMPARE(p1.background, pRef.background);
    QCOMPARE(p2.background, pRef.background);
    QVERIFY(!p1.isDark);
    QVERIFY(!p2.isDark);
}

void EditorPaletteTest::light_themes_have_light_background() {
    // The three "light" themes report isDark == false.
    QCOMPARE(mdreader::theme::paletteForTheme("github").isDark,          false);
    QCOMPARE(mdreader::theme::paletteForTheme("solarized-light").isDark, false);
}

void EditorPaletteTest::dark_themes_have_dark_background() {
    // The four "dark" themes report isDark == true.
    QCOMPARE(mdreader::theme::paletteForTheme("github-dark").isDark,   true);
    QCOMPARE(mdreader::theme::paletteForTheme("dracula").isDark,       true);
    QCOMPARE(mdreader::theme::paletteForTheme("solarized-dark").isDark, true);
    QCOMPARE(mdreader::theme::paletteForTheme("one-dark").isDark,       true);
}

void EditorPaletteTest::light_vs_dark_contrast() {
    // Sanity check on the brightness heuristic — light themes use a
    // background whose lightness is > 0.5, dark themes use one whose
    // lightness is < 0.5. If a future palette entry breaks this we want
    // the test to scream before the user is forced to read white-on-white.
    auto light = mdreader::theme::paletteForTheme("github");
    auto dark  = mdreader::theme::paletteForTheme("github-dark");
    QVERIFY(light.background.lightnessF() > 0.5f);
    QVERIFY(dark.background.lightnessF()  < 0.5f);
}

void EditorPaletteTest::selection_colors_contrast_with_background() {
    // Selection highlight must not be the same color as the background —
    // otherwise highlighted text becomes invisible. Both light and dark
    // themes need this property.
    const QStringList themes = {"github", "github-dark", "dracula",
                                "solarized-light", "solarized-dark",
                                "one-dark"};
    for (const auto& t : themes) {
        auto p = mdreader::theme::paletteForTheme(t);
        QVERIFY2(p.selectionBg != p.background,
                 qPrintable(QString("selection bg equals background for %1").arg(t)));
    }
}

QTEST_MAIN(EditorPaletteTest)
#include "EditorPaletteTest.moc"