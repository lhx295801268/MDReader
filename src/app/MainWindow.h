#pragma once
#include <QMainWindow>
#include <QStringList>
#include <QDateTime>
#include <QMessageBox>
#include <memory>
#include "services/FileWatcher.h"

class QMimeData;
class QDropEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QTabWidget;
class DocumentManager;
class DocumentTab;
class RenderCoordinator;
class ThemeMenu;
class FindBar;
class AppTranslator;
class QAction;
namespace Qt { enum class ColorScheme; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void openFileFromCli(const QString& path);

protected:
    void closeEvent(QCloseEvent* e) override;
    // Phase 13: intercept WindowStateChange so a WM-driven maximize (the
    // typical response to a native title-bar double-click) is redirected
    // into WindowFullScreen. The actual decision logic lives in
    // app/TitleBarFullscreen.h so it can be unit-tested without spinning
    // up the full MainWindow + QWebEngineView stack.
    void changeEvent(QEvent* e) override;
    // Phase 13: Esc exits fullscreen. Standard Qt convention (matches
    // VLC, browsers, most media apps). Conflicts with Ctrl+F? No — Esc
    // reaches MainWindow only when no child widget has captured it; the
    // FindBar's own Esc shortcut handles itself first when visible.
    void keyPressEvent(QKeyEvent* e) override;

    // Drag-and-drop: accept external Markdown files dropped from a file
    // manager. Filtering lives in src/app/DropHandler.h so it's unit
    // tested without spinning up the MainWindow GUI stack. Only .md-ish
    // files are opened; non-files / folders / remote URLs are ignored.
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;

    // Catch-all event filter for double-click → fullscreen toggle on
    // "background" areas of the window (toolbar empty area, status bar,
    // any place that doesn't have a meaningful interactive widget). We
    // filter on the QToolBar's events because that's the most prominent
    // blank space the user might double-click. Action buttons are
    // excluded so their normal click handling still works.
    bool eventFilter(QObject* watched, QEvent* e) override;

private slots:
    void newDocument();
    void openDocument();
    bool saveCurrent();
    void exportCurrentPdf();
    void exportCurrentHtml();
    void toggleOutline();
    void toggleInfo();
    void onTabChanged(int /*idx*/);

    // Phase 4: FileWatcher → MainWindow bridge. Invoked on the main thread
    // (FileWatcher bounces through QMetaObject::invokeMethod). Decides
    // between silent reload and the 3-button conflict modal based on the
    // Document::dirty() state.
    void onExternalChange(const QString& path, const QByteArray& bytes);

    // Phase 7: re-render every tab if we're in follow-system mode. Triggered
    // by QStyleHints::colorSchemeChanged. No-op when the user has pinned a
    // specific theme.
    void onSystemColorSchemeChanged(Qt::ColorScheme scheme);

private:
    void buildUi();
    void loadSettings();
    void saveSettings();

    // Task 20: connect outline ↔ editor ↔ preview bidirectional sync for a
    // newly-created tab. Called by both documentLoaded and newDocument.
    void wireTabSync(DocumentTab* tab);

    // Phase 7: translate (themeMode_, userTheme_, system colorScheme) into
    // the concrete theme basename used by RenderCoordinator. Implementation
    // lives in src/app/ThemeResolution.h so unit tests can exercise it
    // without linking the entire MainWindow translation unit.
    QString resolveEffectiveTheme() const;

    // Phase 7: re-render every open tab with `effectiveTheme`. Used after a
    // user picks a new theme OR after the OS color scheme changes while in
    // follow-system mode.
    void rerenderAllTabs(const QString& effectiveTheme);

    // Phase 12: paint every editor in every tab with the palette matching
    // `theme_`. Called when the user picks a new theme, when the OS color
    // scheme flips while in follow-system mode, and on construction so a
    // freshly-opened editor starts with the right colors.
    void applyEditorTheme();

    // Phase 10: refresh every toolbar action's text after the user
    // switches language. Qt's automatic LanguageChange handling covers
    // QPushButton / QLabel etc., but QAction's text() doesn't get re-run
    // unless we explicitly call setText() again. Called from the language
    // switcher's lambda.
    void retranslateToolbar();

    DocumentTab* currentTab() const;

    // Phase 4: linear scan of tabs_ to find the DocumentTab whose editor is
    // bound to a document at `path`. Returns nullptr if no match — caller
    // must tolerate that (e.g. the tab was closed mid-flight).
    DocumentTab* tabForDocument_byPath(const QString& path) const;

    QTabWidget* tabs_ = nullptr;
    DocumentManager* dm_ = nullptr;
    RenderCoordinator* rc_ = nullptr;
    FileWatcher* fileWatcher_ = nullptr;
    QStringList lastFiles_;
    int currentIndex_ = -1;
    bool outlineVisible_ = true;
    bool infoVisible_ = true;
    // Phase 7: themeMode_ is "auto" (follow OS) or "manual" (use userTheme_).
    // userTheme_ remembers the last concrete theme the user picked so that
    // switching back from auto to manual restores their preference.
    QString themeMode_ = "manual";
    QString userTheme_ = "github";
    // Legacy alias kept for the few call sites that just want "what to
    // render with right now" — equivalent to resolveEffectiveTheme().
    QString theme_ = "github";
    QString renderMode_ = "live";
    bool showLineNumbers_ = true;
    QAction* outlineAct_ = nullptr;
    QAction* infoAct_ = nullptr;
    QAction* liveAct_ = nullptr;
    QAction* manualAct_ = nullptr;
    QAction* refreshAct_ = nullptr;
    // Phase 10: every toolbar action lives on this list, so
    // retranslateToolbar() can call setText(tr(...)) on each one when the
    // user switches language. Without this list, the toolbar text would be
    // frozen at the language that was active when buildUi() ran.
    QList<QAction*> toolbarActions_;
    // Phase 10: language switcher actions (English / 简体中文). Kept as
    // members so retranslateToolbar() can update their check state when
    // the active language flips (and so we can connect them once at
    // construction time without losing the pointers).
    QAction* langEnAct_ = nullptr;
    QAction* langZhAct_ = nullptr;
    ThemeMenu* themeMenu_ = nullptr;
    // Phase 12: docked Ctrl+F find bar. Hidden until the user invokes
    // Ctrl+F, then shown above the central area. Its visible state is
    // driven entirely from the Ctrl+F handler — no action exposed in the
    // toolbar to keep the chrome uncluttered.
    FindBar* findBar_ = nullptr;
    // Phase 10: in-app English/Chinese translator. Owned by QApplication
    // (we install it via QApplication::installTranslator in the ctor);
    // MainWindow holds a raw pointer to call setLanguage() /
    // retranslateRegistered() when the user picks a different language
    // from the toolbar.
    AppTranslator* translator_ = nullptr;
    QByteArray splitterA_state_;
    QByteArray splitterB_state_;
    QByteArray splitterC_state_;
};