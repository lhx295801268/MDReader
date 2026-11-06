#include <QtTest>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QKeySequence>
#include "app/MainWindow.h"

// Phase 14 (macOS): the menu bar is the piece that makes MDReader feel
// like a native Mac application — on macOS QMainWindow's menu bar is
// promoted to the *system* menu bar, so it carries the application menu
// (About / Hide / Quit) plus File / Edit / View / Window.
//
// The structure is platform-independent, so it can be asserted on every
// platform: what changes between them is only which menus exist (the
// application and Window menus are macOS-only).
class MenuBarTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void has_core_menus();
    void file_menu_has_standard_shortcuts();
    void edit_menu_has_standard_entries();
    void view_menu_has_theme_and_language_submenus();
    void theme_picked_in_menu_ticks_the_toolbar_dropdown();
    void language_switch_retranslates_the_menu_bar();

private:
    // QMenuBar and QMenu share the same "actions()" shape, so one
    // template covers both.
    template <typename Container>
    static QMenu* subMenu(Container* parent, const QString& title) {
        if (!parent) return nullptr;
        for (QAction* a : parent->actions()) {
            QMenu* m = a->menu();
            if (m && m->title() == title) return m;
        }
        return nullptr;
    }
    static QAction* actionIn(QMenu* menu, const QString& text);

    MainWindow* w_ = nullptr;
};

// Use a dedicated QSettings scope so the test never picks up (or
// overwrites) a real MDReader session.
void MenuBarTest::initTestCase() {
    QCoreApplication::setOrganizationName("MDReaderTest");
    QCoreApplication::setApplicationName("MenuBarTest");
    w_ = new MainWindow;
}

void MenuBarTest::cleanupTestCase() {
    delete w_;
    w_ = nullptr;
}

QAction* MenuBarTest::actionIn(QMenu* menu, const QString& text) {
    if (!menu) return nullptr;
    for (QAction* a : menu->actions()) {
        if (a->text() == text) return a;
    }
    return nullptr;
}

void MenuBarTest::has_core_menus() {
    QVERIFY(w_);
    QMenuBar* mb = w_->menuBar();
    QVERIFY(mb);
    QStringList titles;
    for (QAction* a : mb->actions()) {
        if (a->menu()) titles << a->menu()->title();
    }
    QVERIFY2(titles.contains("File"), qPrintable(titles.join(", ")));
    QVERIFY2(titles.contains("Edit"), qPrintable(titles.join(", ")));
    QVERIFY2(titles.contains("View"), qPrintable(titles.join(", ")));
#ifdef Q_OS_MACOS
    // macOS adds the application menu (titled after the app) and a
    // Window menu.
    QVERIFY2(titles.contains("MDReader"), qPrintable(titles.join(", ")));
    QVERIFY2(titles.contains("Window"), qPrintable(titles.join(", ")));
#endif
}

void MenuBarTest::file_menu_has_standard_shortcuts() {
    QMenu* file = subMenu(w_->menuBar(), "File");
    QVERIFY(file);
    // The platform conventions come from QKeySequence, so Cmd+N on macOS
    // and Ctrl+N elsewhere.
    QAction* newAct = actionIn(file, "New");
    QVERIFY(newAct);
    QCOMPARE(newAct->shortcut(), QKeySequence(QKeySequence::New));
    QAction* openAct = actionIn(file, "Open");
    QVERIFY(openAct);
    QCOMPARE(openAct->shortcut(), QKeySequence(QKeySequence::Open));
    QAction* saveAct = actionIn(file, "Save");
    QVERIFY(saveAct);
    QCOMPARE(saveAct->shortcut(), QKeySequence(QKeySequence::Save));
    QAction* closeAct = actionIn(file, "Close Tab");
    QVERIFY(closeAct);
    QCOMPARE(closeAct->shortcut(), QKeySequence(QKeySequence::Close));
}

void MenuBarTest::edit_menu_has_standard_entries() {
    QMenu* edit = subMenu(w_->menuBar(), "Edit");
    QVERIFY(edit);
    for (const char* t : {"Undo", "Redo", "Cut", "Copy", "Paste", "Select All"}) {
        QVERIFY2(actionIn(edit, t) != nullptr, t);
    }
    QAction* find = actionIn(edit, QStringLiteral("Find…"));
    QVERIFY(find);
    QCOMPARE(find->shortcut(), QKeySequence(QKeySequence::Find));
}

void MenuBarTest::view_menu_has_theme_and_language_submenus() {
    QMenu* view = subMenu(w_->menuBar(), "View");
    QVERIFY(view);
    QMenu* theme = subMenu(view, "Theme");
    QVERIFY(theme);
    // "Follow System" plus the six concrete themes.
    QCOMPARE(theme->actions().size(), 7);
    QVERIFY(actionIn(theme, "Follow System"));
    QVERIFY(actionIn(theme, "github-dark"));
    QMenu* lang = subMenu(view, "Language");
    QVERIFY(lang);
    QCOMPARE(lang->actions().size(), 2);
}

void MenuBarTest::theme_picked_in_menu_ticks_the_toolbar_dropdown() {
    QMenu* view = subMenu(w_->menuBar(), "View");
    QMenu* theme = subMenu(view, "Theme");
    QVERIFY(theme);
    QAction* dracula = actionIn(theme, "dracula");
    QVERIFY(dracula);
    dracula->trigger();
    QVERIFY(dracula->isChecked());

    // The toolbar's theme dropdown is a different set of actions (a QMenu
    // can only live in one parent menu), so the two have to be kept in
    // sync by hand — check that it happened.
    QMenu* toolbarTheme = nullptr;
    for (QToolBar* tb : w_->findChildren<QToolBar*>()) {
        for (QAction* a : tb->actions()) {
            if (a->menu() && a->menu() != theme) { toolbarTheme = a->menu(); break; }
        }
        if (toolbarTheme) break;
    }
    QVERIFY(toolbarTheme);
    QAction* toolbarDracula = actionIn(toolbarTheme, "dracula");
    QVERIFY(toolbarDracula);
    QVERIFY2(toolbarDracula->isChecked(), "toolbar theme dropdown not synced");
}

void MenuBarTest::language_switch_retranslates_the_menu_bar() {
    QMenu* view = subMenu(w_->menuBar(), "View");
    QMenu* lang = subMenu(view, "Language");
    QVERIFY(lang);
    QAction* zh = actionIn(lang, QString::fromUtf8("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"));
    QVERIFY(zh);
    zh->trigger();

    // The menu titles are re-translated when the language flips.
    QMenu* file = subMenu(w_->menuBar(), QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6"));
    QVERIFY2(file != nullptr, "File menu was not re-translated to Chinese");
    QVERIFY(actionIn(file, QString::fromUtf8("\xe6\x96\xb0\xe5\xbb\xba")));  // 新建

    // ...and switching back restores the English titles.
    QAction* en = actionIn(lang, "English");
    QVERIFY(en);
    en->trigger();
    QVERIFY(subMenu(w_->menuBar(), "File"));
}

QTEST_MAIN(MenuBarTest)
#include "MenuBarTest.moc"
