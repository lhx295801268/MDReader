#include <QtTest>
#include <QAction>
#include <QSignalSpy>
#include "ui/ThemeMenu.h"

class ThemeMenuTest : public QObject {
    Q_OBJECT
private slots:
    void emits_kAuto_when_follow_system_triggered();
    void emits_concrete_name_when_concrete_theme_triggered();
    void setCurrent_auto_checks_only_follow_system();
    void setCurrent_concrete_unchecks_follow_system();
    void available_lists_just_concrete_themes();
};

// Helper: find the action whose visible text starts with `prefix`. Used
// because the "Follow System" text is wrapped in tr() and could vary by
// locale — matching by exact text would be brittle. We rely on the
// implementation adding the auto entry FIRST (before the concrete themes),
// so finding the first checkable action gives us Follow System.
static QAction* findActionByPrefix(const ThemeMenu& m, const QString& prefix) {
    for (auto* a : m.actions()) {
        if (a->text().startsWith(prefix)) return a;
    }
    return nullptr;
}

void ThemeMenuTest::emits_kAuto_when_follow_system_triggered() {
    ThemeMenu m;
    QSignalSpy spy(&m, &ThemeMenu::themeSelected);
    auto* autoAct = findActionByPrefix(m, QStringLiteral("Follow"));
    QVERIFY(autoAct != nullptr);
    autoAct->trigger();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QString::fromLatin1(ThemeMenu::kAuto));
}

void ThemeMenuTest::emits_concrete_name_when_concrete_theme_triggered() {
    ThemeMenu m;
    QSignalSpy spy(&m, &ThemeMenu::themeSelected);
    // Find the "github" action directly (it lives after the separator + Follow System).
    QAction* gh = nullptr;
    for (auto* a : m.actions()) {
        if (a->text() == QStringLiteral("github")) { gh = a; break; }
    }
    QVERIFY(gh != nullptr);
    gh->trigger();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("github"));
}

void ThemeMenuTest::setCurrent_auto_checks_only_follow_system() {
    ThemeMenu m;
    m.setCurrent(QString::fromLatin1(ThemeMenu::kAuto));
    auto* autoAct = findActionByPrefix(m, QStringLiteral("Follow"));
    QVERIFY(autoAct != nullptr);
    QVERIFY(autoAct->isChecked());
    // The exclusive QActionGroup must have unchecked every other entry.
    for (auto* a : m.actions()) {
        if (a == autoAct) continue;
        QVERIFY2(!a->isChecked(), qPrintable(QStringLiteral("expected %1 unchecked").arg(a->text())));
    }
}

void ThemeMenuTest::setCurrent_concrete_unchecks_follow_system() {
    ThemeMenu m;
    m.setCurrent(QStringLiteral("dracula"));
    auto* autoAct = findActionByPrefix(m, QStringLiteral("Follow"));
    QVERIFY(autoAct != nullptr);
    QVERIFY(!autoAct->isChecked());
    // dracula itself should be checked.
    bool draculaChecked = false;
    for (auto* a : m.actions()) {
        if (a->text() == QStringLiteral("dracula")) {
            draculaChecked = a->isChecked();
            break;
        }
    }
    QVERIFY(draculaChecked);
}

void ThemeMenuTest::available_lists_just_concrete_themes() {
    // The kAuto sentinel must never appear in the concrete-theme list — it
    // is its own slot in the menu.
    ThemeMenu m;
    for (const auto& t : m.available()) {
        QVERIFY(t != QString::fromLatin1(ThemeMenu::kAuto));
    }
    QVERIFY(m.available().contains(QStringLiteral("github")));
    QVERIFY(m.available().contains(QStringLiteral("github-dark")));
}

// QTEST_GUILESS_MAIN creates QGuiApplication, but QMenu is a QWidget and
// QWidget refuses to construct under anything less than a full QApplication.
// Use a custom main so we get a real QApplication, then drive the test the
// same way QTEST_MAIN would.
#include <QApplication>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ThemeMenuTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "ThemeMenuTest.moc"