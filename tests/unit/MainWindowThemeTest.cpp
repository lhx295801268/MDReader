#include <QtTest>
#include "app/ThemeResolution.h"
using mdreader::theme::SystemColorScheme;

// Phase 7: resolveTheme() is a pure (mode, userTheme, systemScheme) →
// concrete theme function. Lives in a header so we don't have to link
// MainWindow.cpp (which transitively pulls in QWebEngineWidgets, cmark-gfm,
// every Document* / Render* / FileWatcher, etc.) just to test a 4-line
// pure function.
class MainWindowThemeTest : public QObject {
    Q_OBJECT
private slots:
    void auto_light_returns_light_theme();
    void auto_dark_returns_dark_theme();
    void auto_unknown_returns_light_theme();
    void manual_returns_userTheme_unchanged();
    void manual_ignores_system_scheme();
};

void MainWindowThemeTest::auto_light_returns_light_theme() {
    QCOMPARE(mdreader::theme::resolveTheme(QStringLiteral("auto"),
                                           QStringLiteral("dracula"),
                                           SystemColorScheme::Light),
             QStringLiteral("github"));
}

void MainWindowThemeTest::auto_dark_returns_dark_theme() {
    QCOMPARE(mdreader::theme::resolveTheme(QStringLiteral("auto"),
                                           QStringLiteral("github"),
                                           SystemColorScheme::Dark),
             QStringLiteral("github-dark"));
}

void MainWindowThemeTest::auto_unknown_returns_light_theme() {
    // Qt's Unknown is the transient startup value before the platform has
    // been probed — must not crash, must not silently pick dark.
    QCOMPARE(mdreader::theme::resolveTheme(QStringLiteral("auto"),
                                           QStringLiteral("one-dark"),
                                           SystemColorScheme::Unknown),
             QStringLiteral("github"));
}

void MainWindowThemeTest::manual_returns_userTheme_unchanged() {
    QCOMPARE(mdreader::theme::resolveTheme(QStringLiteral("manual"),
                                           QStringLiteral("solarized-dark"),
                                           SystemColorScheme::Light),
             QStringLiteral("solarized-dark"));
}

void MainWindowThemeTest::manual_ignores_system_scheme() {
    // Manual mode must not even consult the system scheme — even when the
    // OS flips to dark, a user-picked concrete theme must stick.
    QCOMPARE(mdreader::theme::resolveTheme(QStringLiteral("manual"),
                                           QStringLiteral("github-light-attempt"),
                                           SystemColorScheme::Dark),
             QStringLiteral("github-light-attempt"));
}

QTEST_APPLESS_MAIN(MainWindowThemeTest)
#include "MainWindowThemeTest.moc"