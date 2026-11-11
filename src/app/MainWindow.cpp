#include "app/MainWindow.h"
#include "app/DropHandler.h"
#include "app/ThemeResolution.h"
#include "app/EditorPalette.h"
#include "documents/Document.h"
#include "documents/DocumentManager.h"
#include "ui/DocumentTab.h"
#include "ui/EditorView.h"
#include "ui/PreviewView.h"
#include "ui/OutlineView.h"
#include "ui/InfoView.h"
#include "ui/ThemeMenu.h"
#include "ui/FindBar.h"
#include "services/RenderCoordinator.h"
#include "services/WordCounter.h"
#include "services/OutlineExtractor.h"
#include "services/AppTranslator.h"
#include <QApplication>
#include <QTabWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QFileOpenEvent>
#include <QWebEngineView>
#include <QMouseEvent>
#include <QFileDialog>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QPointer>
#include <QActionGroup>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QTextCursor>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStyleHints>
#include <QGuiApplication>
#include <QShortcut>
#include <QKeySequence>
#include "services/FileWatcher.h"
#include "app/TitleBarFullscreen.h"
#include <QEvent>
#include <QKeyEvent>

// The version is injected by CMake (see src/CMakeLists.txt). The fallback
// keeps this file compilable outside that build system.
#ifndef MDREADER_VERSION
#define MDREADER_VERSION "0.0.0"
#endif

namespace {
constexpr auto kKeyLastFiles       = "session/lastFiles";
constexpr auto kKeyCurrentIndex    = "session/currentIndex";
constexpr auto kKeyTheme           = "ui/theme";
constexpr auto kKeyThemeMode       = "ui/themeMode";
constexpr auto kKeyRenderMode      = "ui/renderMode";
constexpr auto kKeyShowLineNumbers = "ui/showLineNumbers";
constexpr auto kKeySplitterA       = "layout/splitterA";
constexpr auto kKeySplitterB       = "layout/splitterB";
constexpr auto kKeySplitterC       = "layout/splitterC";
constexpr auto kKeyOutlineVisible  = "sidebar/outlineVisible";
constexpr auto kKeyInfoVisible     = "sidebar/infoVisible";
constexpr auto kValueLive          = "live";
constexpr auto kValueManual        = "manual";
constexpr auto kValueThemeManual   = "manual";   // distinguishes from renderMode kValueManual

// kValueAuto / kAutoLightTheme / kAutoDarkTheme live in
// app/ThemeResolution.h (so unit tests can reach them without linking
// MainWindow.cpp). Aliases here are just for readability inside this TU.
using mdreader::theme::kModeAuto;
using mdreader::theme::kAutoLightTheme;
using mdreader::theme::kAutoDarkTheme;

// Phase 14 (macOS): name of the dynamic property that carries the English
// source text of a menu entry. QAction::data() is already taken by the
// theme id on some entries, so the menu strings live in their own
// property.
constexpr auto kMenuSrcProp = "mdr_src";

// Phase 14 (macOS): re-run the translation for every string inside a menu,
// recursing into submenus. QAction::setText() is not refreshed by Qt's
// automatic LanguageChange handling, so the language switcher calls this
// by hand.
void retranslateMenuRecursive(QMenu* menu) {
    if (!menu) return;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator()) continue;
        QString src = a->property(kMenuSrcProp).toString();
        if (src.isEmpty()) src = a->data().toString();
        if (!src.isEmpty()) {
            a->setText(
                QCoreApplication::translate("MainWindow", src.toUtf8().constData()));
        }
        if (QMenu* sub = a->menu()) retranslateMenuRecursive(sub);
    }
}

// Phase 14 (macOS): run `fnEditor` on the focused Markdown editor, or
// `fnPage` on the focused preview's page — whichever has the keyboard
// focus. The Edit menu (Undo / Redo / Cut / Copy / Paste / Select All)
// uses this so one set of menu entries serves both panes.
template <typename FnEditor, typename FnPage>
void withEditTarget(FnEditor&& fnEditor, FnPage&& fnPage) {
    QWidget* w = QApplication::focusWidget();
    if (auto* ed = qobject_cast<QPlainTextEdit*>(w)) {
        fnEditor(ed);
        return;
    }
    if (auto* view = qobject_cast<QWebEngineView*>(w)) {
        if (QWebEnginePage* page = view->page()) fnPage(page);
    }
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MDReader");
    resize(1280, 800);
    // Phase 8: accept external Markdown files dragged from a file manager.
    // The actual filtering (.md extension, local files only) happens in
    // dragEnterEvent → DropHandler::canAccept(); dropEvent runs the file
    // list through DocumentManager::openFile().
    setAcceptDrops(true);
    dm_ = new DocumentManager(this);
    rc_ = new RenderCoordinator(this);
    // Phase 10: install the in-app translator so all tr() calls below
    // resolve through our English↔Chinese table. QApplication takes
    // ownership of the translator. We also keep a raw pointer for the
    // language switcher in the toolbar to call setLanguage() +
    // retranslateRegistered() when the user picks a different language.
    translator_ = new AppTranslator(this);
    QApplication::installTranslator(translator_);
    // Phase 4: external-change notifier. Watches each opened document's
    // path; emits externalModified(path, bytes) after debounce when the file
    // is rewritten on disk by another process. We react in onExternalChange.
    fileWatcher_ = new FileWatcher(this);
    connect(fileWatcher_, &FileWatcher::externalModified,
            this, &MainWindow::onExternalChange);
    // Phase 7: follow the OS color scheme. The slot is harmless when the
    // user has pinned a specific theme (themeMode_ == "manual") — it just
    // recomputes theme_ and skips the rerender.
    // Requires Qt 6.5+ (QStyleHints::colorSchemeChanged). On older Qt we
    // fall back to manual-only mode: the resolver still returns the user's
    // pinned theme, just without auto-tracking OS flips.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(),
            &QStyleHints::colorSchemeChanged,
            this, &MainWindow::onSystemColorSchemeChanged);
#endif
    buildUi();
    loadSettings();
    // Defense-in-depth: loadSettings() already drives setMode() via the
    // liveAct_/manualAct_ toggled lambdas; this keeps rc_ consistent
    // if loadSettings() is later refactored.
    rc_->setMode(renderMode_ == kValueManual ? RenderCoordinator::Manual : RenderCoordinator::Live);

    // Sync toolbar action state with persisted visibility without firing
    // toggle signals (which would otherwise re-set outlineVisible_/infoVisible_
    // back to whatever buildUi() initialized them to).
    outlineAct_->blockSignals(true);
    infoAct_->blockSignals(true);
    outlineAct_->setChecked(outlineVisible_);
    infoAct_->setChecked(infoVisible_);
    outlineAct_->blockSignals(false);
    infoAct_->blockSignals(false);

    // Capture saved index + expected count by value so the async restore
    // doesn't race against live currentIndex_ updates.
    const int savedIndex = currentIndex_;
    const int expectedCount = lastFiles_.size();
    QPointer<MainWindow> self = this;
    connect(dm_, &DocumentManager::documentLoaded, this,
            [self, savedIndex, expectedCount](std::shared_ptr<Document> doc) {
        if (!self) return;
        // Dedup at the UI layer too: if a tab for this path already exists
        // (e.g. user double-clicked the same .md again, or another path
        // resolved to the same canonical file via symlink), just focus it.
        // DocumentManager::openFile has its own check but it runs on a
        // worker thread; this one is deterministic on the main thread
        // before we allocate a DocumentTab.
        if (auto* existing = self->tabForDocument_byPath(doc->path())) {
            self->tabs_->setCurrentWidget(existing);
            return;
        }
        auto* tab = new DocumentTab(doc);
        // Task 20: pass the tab so RenderCoordinator can dispatch outline +
        // stats back to it after each render.
        self->rc_->bind(doc->path(), tab->preview(), tab);
        // Kick off the first render for this document. Without this call,
        // the preview stays empty — bind() only registers the view with
        // the coordinator, it doesn't dispatch a render job. Manual refresh,
        // file-watcher updates, and theme rerenders all go through this
        // same requestRender path, so the initial render reuses the same
        // pipeline (worker thread → postToMain → PreviewView::setMarkdownHtml).
        self->rc_->requestRender(doc->path(), doc->text(), self->theme_,
                                 /*force=*/true);
        // Outline click → editor cursor + preview scroll;
        // editor cursor → outline highlight.
        self->wireTabSync(tab);
        // Phase 12: paint the new editor with the active theme palette so
        // it matches the right-hand preview from the moment the tab
        // appears, rather than after the user touches the theme menu.
        tab->editor()->applyEditorPalette(
            mdreader::theme::paletteForTheme(self->theme_));
        self->tabs_->addTab(tab, QFileInfo(doc->path()).fileName());
        // Phase 4: start watching the file so we can react when something
        // else rewrites it on disk. Skip empty paths (newDocument path —
        // those have nothing on disk to watch).
        if (!doc->path().isEmpty()) self->fileWatcher_->watch(doc->path());
        if (self->tabs_->count() == expectedCount && savedIndex >= 0
            && savedIndex < self->tabs_->count()) {
            self->tabs_->setCurrentIndex(savedIndex);
        }
    });
}

void MainWindow::buildUi() {
    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    connect(tabs_, &QTabWidget::tabCloseRequested,
            this, &MainWindow::closeTabAt);
    setCentralWidget(tabs_);

    auto* tb = addToolBar("Main");
    // Phase 9: double-click on the toolbar's empty area (the bit of space
    // to the right of the action buttons, or anywhere outside the action
    // group) toggles fullscreen. The eventFilter checks whether the dbl-click
    // landed on a QToolButton (action) and skips if so, so the buttons
    // keep their normal click behavior.
    tb->installEventFilter(this);
    // Phase 10: every visible string in the toolbar is wrapped in tr() so
    // AppTranslator can substitute the Chinese mapping on LanguageChange.
    // New strings should be added both here AND to AppTranslator::zhTable_.
    // Phase 14 (macOS): these live in the toolbar *and* in the menu bar.
    // QKeySequence::New / Open / Save / Refresh / Close map to the
    // platform's own conventions automatically — Cmd+N on macOS,
    // Ctrl+N on Linux and Windows — so the same action object gives a
    // macOS user the shortcuts they expect without a single #ifdef.
    newAct_ = tb->addAction(tr("New"));
    newAct_->setShortcut(QKeySequence::New);
    openAct_ = tb->addAction(tr("Open"));
    openAct_->setShortcut(QKeySequence::Open);
    saveAct_ = tb->addAction(tr("Save"));
    saveAct_->setShortcut(QKeySequence::Save);
    tb->addSeparator();
    pdfAct_ = tb->addAction(tr("Export PDF"));
    htmlAct_ = tb->addAction(tr("Export HTML"));
    tb->addSeparator();
    outlineAct_ = tb->addAction(tr("Outline"));
    outlineAct_->setCheckable(true); outlineAct_->setChecked(true);
    infoAct_ = tb->addAction(tr("Info"));
    infoAct_->setCheckable(true); infoAct_->setChecked(true);

    themeMenu_ = new ThemeMenu(this);
    themeBtn_ = tb->addAction(tr("Theme"));
    themeBtn_->setMenu(themeMenu_);
    // Force InstantPopup on the toolbar button so a single click shows
    // the theme menu (default DelayedPopup requires press-and-hold).
    for (QObject* child : tb->children()) {
        auto* tbBtn = qobject_cast<QToolButton*>(child);
        if (!tbBtn) continue;
        if (tbBtn->defaultAction() == themeBtn_) {
            tbBtn->setPopupMode(QToolButton::InstantPopup);
            break;
        }
    }
    connect(themeMenu_, &ThemeMenu::themeSelected,
            this, &MainWindow::selectTheme);

    tb->addSeparator();
    liveAct_ = tb->addAction(tr("Live"));
    liveAct_->setCheckable(true); liveAct_->setChecked(true);
    manualAct_ = tb->addAction(tr("Manual"));
    manualAct_->setCheckable(true);
    QActionGroup* grp = new QActionGroup(this);
    grp->addAction(liveAct_); grp->addAction(manualAct_);
    grp->setExclusive(true);

    connect(liveAct_, &QAction::toggled, this, [this](bool on) {
        if (on) { rc_->setMode(RenderCoordinator::Live); renderMode_ = kValueLive; }
        refreshAct_->setEnabled(!on);
    });
    connect(manualAct_, &QAction::toggled, this, [this](bool on) {
        if (on) { rc_->setMode(RenderCoordinator::Manual); renderMode_ = kValueManual; }
    });

    refreshAct_ = tb->addAction(tr("Refresh"));
    // Phase 10: language switcher. Sits at the right edge of the toolbar
    // so it doesn't disrupt the natural left-to-right action layout. Two
    // entries: English (default) and 简体中文.
    tb->addSeparator();
    langBtn_ = tb->addAction(tr("Language"));
    QMenu* langMenu = new QMenu(this);
    langEnAct_ = langMenu->addAction(tr("English"));
    langZhAct_ = langMenu->addAction(tr("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87")); // 简体中文
    langEnAct_->setCheckable(true); langEnAct_->setChecked(true);
    langZhAct_->setCheckable(true);
    QActionGroup* langGrp = new QActionGroup(this);
    langGrp->addAction(langEnAct_); langGrp->addAction(langZhAct_);
    langGrp->setExclusive(true);
    langBtn_->setMenu(langMenu);
    // Some Qt styles need an explicit popup mode to make a QAction with a
    // menu attached behave as a menu-button on the toolbar. The default
    // (DelayedPopup) shows the menu on press-and-hold; InstantPopup
    // shows it on a single click, which matches what users expect for a
    // language switcher. Find the QToolButton Qt creates internally for
    // `langBtn` and set its popupMode.
    for (QObject* child : tb->children()) {
        auto* tbBtn = qobject_cast<QToolButton*>(child);
        if (!tbBtn) continue;
        if (tbBtn->defaultAction() == langBtn_) {
            tbBtn->setPopupMode(QToolButton::InstantPopup);
            break;
        }
    }
    connect(langEnAct_, &QAction::triggered, this, [this]() {
        applyLanguage(AppTranslator::English);
    });
    connect(langZhAct_, &QAction::triggered, this, [this]() {
        applyLanguage(AppTranslator::Chinese);
    });

    // Phase 10: stash each toolbar action's source English text into
    // QAction::data() so retranslateToolbar() can call tr() again with the
    // same source literal after a language flip. Without this, the toolbar
    // text is frozen at whatever language was active during buildUi().
    auto stash = [](QAction* a, const char* src) {
        if (a) a->setData(QString::fromUtf8(src));
    };
    stash(newAct_,    "New");
    stash(openAct_,   "Open");
    stash(saveAct_,   "Save");
    stash(pdfAct_,    "Export PDF");
    stash(htmlAct_,   "Export HTML");
    stash(outlineAct_, "Outline");
    stash(infoAct_,  "Info");
    stash(themeBtn_,  "Theme");
    stash(liveAct_,  "Live");
    stash(manualAct_,"Manual");
    stash(refreshAct_,"Refresh");
    stash(langBtn_,   "Language");
    stash(langEnAct_,"English");
    // langZhAct_ uses 简体中文 as its source literal (no English fallback
    // needed — that's the canonical Chinese display name).
    refreshAct_->setEnabled(false);  // disabled while in Live mode (no Refresh needed)
    refreshAct_->setShortcut(QKeySequence::Refresh);
    connect(refreshAct_, &QAction::triggered, this, [this] {
        auto* tab = currentTab();
        if (!tab) return;
        auto doc = tab->editor()->document();
        if (!doc) return;
        rc_->requestRender(doc->path(), doc->text(), theme_, /*force=*/true);
    });

    connect(newAct_, &QAction::triggered, this, &MainWindow::newDocument);
    connect(openAct_, &QAction::triggered, this, &MainWindow::openDocument);
    connect(saveAct_, &QAction::triggered, this, &MainWindow::saveCurrent);
    connect(pdfAct_, &QAction::triggered, this, &MainWindow::exportCurrentPdf);
    connect(htmlAct_, &QAction::triggered, this, &MainWindow::exportCurrentHtml);
    connect(outlineAct_, &QAction::toggled, this, &MainWindow::toggleOutline);
    connect(infoAct_, &QAction::toggled, this, &MainWindow::toggleInfo);

    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    // Phase 12: dock the in-editor find bar above the central widget so
    // it sits visually between the toolbar and the editor. We use a
    // dedicated QToolBar rather than addWidget() to the existing main
    // toolbar because the main toolbar already has its own dbl-click
    // fullscreen event filter and we don't want the find bar to inherit
    // that quirk. The find toolbar itself is non-movable / non-floatable
    // so it can't be accidentally dragged away.
    auto* findToolBar = addToolBar("Find");
    findToolBar->setMovable(false);
    findToolBar->setFloatable(false);
    findToolBar->setIconSize(QSize(16, 16));
    findBar_ = new FindBar(findToolBar);
    findToolBar->addWidget(findBar_);
    findToolBar->hide();  // shown on Ctrl+F

    // Ctrl+F (Cmd+F on macOS) opens the find bar (or refocuses its input
    // if already open). We use a QAction instead of a bare QShortcut: a
    // menu action carries the shortcut *and* shows it in the menu, which
    // is what a macOS user expects from the Edit menu.
    findAct_ = new QAction(tr("Find…"), this);
    findAct_->setShortcut(QKeySequence::Find);
    connect(findAct_, &QAction::triggered, this, [this] {
        auto* tab = currentTab();
        if (!tab) return;
        auto* editor = tab->editor();
        if (!editor) return;
        // If the bar is already open, the user almost certainly means
        // "focus the search input so I can type" — re-show and select
        // all rather than dismissing + reopening (which would lose the
        // existing query).
        if (findBar_ && findBar_->isVisibleBar()) {
            findBar_->showFor(editor);
            return;
        }
        findBar_->showFor(editor);
    });
    connect(findBar_, &FindBar::findBarClosed, this, [this] {
        // Move focus back to the current tab's editor; otherwise the
        // user is stranded on the now-hidden bar with no caret.
        if (auto* tab = currentTab()) {
            if (auto* editor = tab->editor()) editor->setFocus();
        }
    });

    // Phase 14 (macOS): build the menu bar last, so it can reference every
    // action created above.
    buildMenuBar();
}

// ---------------------------------------------------------------------------
// Phase 14 (macOS): menu bar
// ---------------------------------------------------------------------------

void MainWindow::buildMenuBar() {
    // QMainWindow::menuBar() returns the window's menu bar. On macOS the
    // Cocoa plugin promotes it to the *system* menu bar — the strip at the
    // top of the screen — which is what a Mac user expects from an
    // application. On Linux and Windows the same menu bar is drawn at the
    // top of the window. Either way the code below is identical.
    QMenuBar* mb = menuBar();
    mb->setNativeMenuBar(true);

    // Stash the English source text of every string we create so the
    // language switcher can re-translate it later (QAction::setText() is
    // not refreshed automatically when the translator is swapped).
    auto stash = [](QAction* a, const char* src) {
        if (a) a->setProperty(kMenuSrcProp, QString::fromUtf8(src));
    };

#ifdef Q_OS_MACOS
    // --- Application menu --------------------------------------------------
    // On macOS the first menu of the menu bar is the application menu: it
    // carries the app's name plus About / Hide / Quit — the entries every
    // Mac app has there and nowhere else.
    QMenu* appMenu = mb->addMenu(tr("MDReader"));
    appMenu->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("MDReader"));
    aboutAct_ = appMenu->addAction(tr("About MDReader"));
    stash(aboutAct_, "About MDReader");
    connect(aboutAct_, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this, tr("About MDReader"),
            tr("MDReader %1\n\nA cross-platform Markdown reader and editor.\n"
               "Built with Qt %2 and cmark-gfm.")
                .arg(QStringLiteral(MDREADER_VERSION),
                     QStringLiteral(QT_VERSION_STR)));
    });
    appMenu->addSeparator();
    hideAct_ = appMenu->addAction(tr("Hide MDReader"));
    stash(hideAct_, "Hide MDReader");
    // Qt::CTRL is the Command key on macOS and the Control key elsewhere,
    // so the same sequence gives ⌘H on a Mac and Ctrl+H on Linux/Windows.
    hideAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    connect(hideAct_, &QAction::triggered, this, &QWidget::hide);
    hideOthersAct_ = appMenu->addAction(tr("Hide Others"));
    stash(hideOthersAct_, "Hide Others");
    hideOthersAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
    connect(hideOthersAct_, &QAction::triggered, this, [this] {
        // "Hide Others" = hide every other window of this app.
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w != this && w->isVisible()) w->hide();
        }
    });
    showAllAct_ = appMenu->addAction(tr("Show All"));
    stash(showAllAct_, "Show All");
    connect(showAllAct_, &QAction::triggered, this, [this] {
        show();
        raise();
        activateWindow();
    });
    appMenu->addSeparator();
    quitAct_ = appMenu->addAction(tr("Quit MDReader"));
    stash(quitAct_, "Quit MDReader");
    quitAct_->setShortcut(QKeySequence::Quit);
    connect(quitAct_, &QAction::triggered, qApp, &QApplication::quit);
#endif

    // --- File --------------------------------------------------------------
    QMenu* fileMenu = mb->addMenu(tr("File"));
    fileMenu->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("File"));
    fileMenu->addAction(newAct_);
    fileMenu->addAction(openAct_);
    fileMenu->addAction(saveAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(pdfAct_);
    fileMenu->addAction(htmlAct_);
    fileMenu->addSeparator();
    closeTabAct_ = fileMenu->addAction(tr("Close Tab"));
    stash(closeTabAct_, "Close Tab");
    closeTabAct_->setShortcut(QKeySequence::Close);
    connect(closeTabAct_, &QAction::triggered, this, [this] {
        if (tabs_) closeTabAt(tabs_->currentIndex());
    });
#ifndef Q_OS_MACOS
    // On macOS Quit lives in the application menu instead.
    fileMenu->addSeparator();
    quitAct_ = fileMenu->addAction(tr("Quit"));
    stash(quitAct_, "Quit");
    quitAct_->setShortcut(QKeySequence::Quit);
    connect(quitAct_, &QAction::triggered, qApp, &QApplication::quit);
#endif

    // --- Edit --------------------------------------------------------------
    // The Undo…Select All entries act on whatever holds the keyboard focus:
    // the Markdown editor (a QPlainTextEdit) or the preview (a
    // QWebEngineView). The editor implements the operations itself; the
    // preview goes through its QWebEnginePage, which exposes them as
    // WebActions.
    QMenu* editMenu = mb->addMenu(tr("Edit"));
    editMenu->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("Edit"));

    undoAct_ = editMenu->addAction(tr("Undo"));
    stash(undoAct_, "Undo");
    undoAct_->setShortcut(QKeySequence::Undo);
    connect(undoAct_, &QAction::triggered, this, [] {
        withEditTarget([](auto* ed) { ed->undo(); },
                       [](auto* page) { page->triggerAction(QWebEnginePage::Undo); });
    });
    redoAct_ = editMenu->addAction(tr("Redo"));
    stash(redoAct_, "Redo");
    redoAct_->setShortcut(QKeySequence::Redo);
    connect(redoAct_, &QAction::triggered, this, [] {
        withEditTarget([](auto* ed) { ed->redo(); },
                       [](auto* page) { page->triggerAction(QWebEnginePage::Redo); });
    });
    editMenu->addSeparator();
    cutAct_ = editMenu->addAction(tr("Cut"));
    stash(cutAct_, "Cut");
    cutAct_->setShortcut(QKeySequence::Cut);
    connect(cutAct_, &QAction::triggered, this, [] {
        withEditTarget([](auto* ed) { ed->cut(); },
                       [](auto* page) { page->triggerAction(QWebEnginePage::Cut); });
    });
    copyAct_ = editMenu->addAction(tr("Copy"));
    stash(copyAct_, "Copy");
    copyAct_->setShortcut(QKeySequence::Copy);
    connect(copyAct_, &QAction::triggered, this, [] {
        withEditTarget([](auto* ed) { ed->copy(); },
                       [](auto* page) { page->triggerAction(QWebEnginePage::Copy); });
    });
    pasteAct_ = editMenu->addAction(tr("Paste"));
    stash(pasteAct_, "Paste");
    pasteAct_->setShortcut(QKeySequence::Paste);
    connect(pasteAct_, &QAction::triggered, this, [] {
        withEditTarget([](auto* ed) { ed->paste(); },
                       [](auto* page) { page->triggerAction(QWebEnginePage::Paste); });
    });
    selectAllAct_ = editMenu->addAction(tr("Select All"));
    stash(selectAllAct_, "Select All");
    selectAllAct_->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct_, &QAction::triggered, this, [] {
        withEditTarget([](auto* ed) { ed->selectAll(); },
                       [](auto* page) { page->triggerAction(QWebEnginePage::SelectAll); });
    });
    editMenu->addSeparator();
    editMenu->addAction(findAct_);

    // --- View --------------------------------------------------------------
    QMenu* viewMenu = mb->addMenu(tr("View"));
    viewMenu->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("View"));
    viewMenu->addAction(outlineAct_);
    viewMenu->addAction(infoAct_);
    viewMenu->addSeparator();

    // View ▸ Theme. A QMenu can only be plugged into one parent menu, so
    // this submenu needs its own actions — they are kept in
    // themeMenubarActions_ and ticked by setThemeChecked().
    QMenu* themeSub = viewMenu->addMenu(tr("Theme"));
    themeSub->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("Theme"));
    QActionGroup* themeGrp = new QActionGroup(themeSub);
    themeGrp->setExclusive(true);
    for (const QString& id : ThemeMenu::entries()) {
        QAction* a = themeSub->addAction(
            id == ThemeMenu::kAuto ? tr("Follow System")
                                   : tr(id.toUtf8().constData()));
        a->setCheckable(true);
        a->setData(id);                              // theme id
        a->setProperty(kMenuSrcProp,                 // source text for retranslate
                       id == ThemeMenu::kAuto ? QStringLiteral("Follow System")
                                             : id);
        themeGrp->addAction(a);
        themeMenubarActions_.append(a);
        connect(a, &QAction::triggered, this, [this, id] { selectTheme(id); });
    }

    viewMenu->addSeparator();
    viewMenu->addAction(liveAct_);
    viewMenu->addAction(manualAct_);
    viewMenu->addAction(refreshAct_);
    viewMenu->addSeparator();

    fullscreenAct_ = viewMenu->addAction(tr("Full Screen"));
    stash(fullscreenAct_, "Full Screen");
    fullscreenAct_->setCheckable(true);
#ifdef Q_OS_MACOS
    // macOS convention: ⌃⌘F (Qt::META is the Control key on macOS).
    fullscreenAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::META | Qt::Key_F));
#else
    fullscreenAct_->setShortcut(QKeySequence(Qt::Key_F11));
#endif
    connect(fullscreenAct_, &QAction::toggled, this, [this](bool on) {
        if (on) {
            setWindowState(windowState() | Qt::WindowFullScreen);
        } else {
            setWindowState(windowState() & ~Qt::WindowFullScreen);
        }
    });
    viewMenu->addSeparator();

    // View ▸ Language — again its own actions, for the same reason as the
    // theme submenu above.
    QMenu* langSub = viewMenu->addMenu(tr("Language"));
    langSub->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("Language"));
    langMenubarEnAct_ = langSub->addAction(tr("English"));
    stash(langMenubarEnAct_, "English");
    langMenubarEnAct_->setCheckable(true);
    langMenubarZhAct_ = langSub->addAction(tr("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"));
    stash(langMenubarZhAct_, "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87");
    langMenubarZhAct_->setCheckable(true);
    QActionGroup* langGrp = new QActionGroup(langSub);
    langGrp->setExclusive(true);
    langGrp->addAction(langMenubarEnAct_);
    langGrp->addAction(langMenubarZhAct_);
    connect(langMenubarEnAct_, &QAction::triggered, this, [this] {
        applyLanguage(AppTranslator::English);
    });
    connect(langMenubarZhAct_, &QAction::triggered, this, [this] {
        applyLanguage(AppTranslator::Chinese);
    });

#ifdef Q_OS_MACOS
    // --- Window ------------------------------------------------------------
    QMenu* windowMenu = mb->addMenu(tr("Window"));
    windowMenu->menuAction()->setProperty(kMenuSrcProp, QStringLiteral("Window"));
    QAction* minimizeAct = windowMenu->addAction(tr("Minimize"));
    stash(minimizeAct, "Minimize");
    minimizeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));   // ⌘M
    connect(minimizeAct, &QAction::triggered, this, &QWidget::showMinimized);
    QAction* bringAllAct = windowMenu->addAction(tr("Bring All to Front"));
    stash(bringAllAct, "Bring All to Front");
    connect(bringAllAct, &QAction::triggered, this, [this] {
        show();
        raise();
        activateWindow();
    });
#endif

    // Keep the Edit entries in sync with whatever has the focus, and
    // reflect the current fullscreen state in the View menu.
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget*, QWidget*) { updateEditActions(); });
    updateEditActions();
    if (fullscreenAct_) {
        fullscreenAct_->setChecked(windowState() & Qt::WindowFullScreen);
    }
    setThemeChecked();
    // The language switcher remembers the choice in the toolbar; mirror it
    // into the menu-bar submenu.
    if (langMenubarEnAct_ && translator_) {
        const bool zh = translator_->currentLanguage() == AppTranslator::Chinese;
        langMenubarEnAct_->setChecked(!zh);
        langMenubarZhAct_->setChecked(zh);
    }
}

void MainWindow::retranslateMenubar() {
    // Same trick as retranslateToolbar(): re-run tr() for every string in
    // the menu bar. The English source text is stashed in the action's
    // "mdr_src" property (or, for the actions shared with the toolbar, in
    // QAction::data()).
    QMenuBar* mb = menuBar();
    for (QAction* a : mb->actions()) {
        QString src = a->property(kMenuSrcProp).toString();
        if (src.isEmpty()) src = a->data().toString();
        QMenu* m = a->menu();
        if (!src.isEmpty()) {
            const QString text =
                QCoreApplication::translate("MainWindow", src.toUtf8().constData());
            if (m) m->setTitle(text);
            else   a->setText(text);
        }
        if (m) retranslateMenuRecursive(m);
    }
    // The theme submenu keeps its ids in QAction::data(), so its labels
    // are re-derived from the id instead of the stashed source text.
    for (QAction* a : themeMenubarActions_) {
        const QString id = a->data().toString();
        a->setText(id == ThemeMenu::kAuto ? tr("Follow System")
                                          : tr(id.toUtf8().constData()));
    }
}

void MainWindow::selectTheme(const QString& t) {
    // Phase 7: "auto" sentinel = follow the OS color scheme. Any other
    // value is a concrete theme basename; remember it so future toggles
    // back to manual mode restore the user's preference.
    if (t == ThemeMenu::kAuto) {
        themeMode_ = kModeAuto;
    } else {
        themeMode_  = kValueThemeManual;
        userTheme_  = t;
    }
    theme_ = resolveEffectiveTheme();
    rerenderAllTabs(theme_);
    // Phase 12: also repaint every editor so the left pane tracks the
    // theme flip, not just the right pane.
    applyEditorTheme();
    // Tick the entry in both the toolbar dropdown and the menu bar.
    setThemeChecked();
}

void MainWindow::setThemeChecked() {
    const QString cur = themeMode_ == kModeAuto ? ThemeMenu::kAuto : userTheme_;
    if (themeMenu_) themeMenu_->setCurrent(cur);
    for (QAction* a : themeMenubarActions_) {
        a->setChecked(a->data().toString() == cur);
    }
}

void MainWindow::applyLanguage(AppTranslator::Language lang) {
    if (!translator_) return;
    qWarning("mdreader: language switch → %s",
             lang == AppTranslator::Chinese ? "Chinese" : "English");
    translator_->setLanguage(lang);
    // Ask every registered widget to re-run its tr() calls …
    translator_->retranslateRegistered();
    // … then fix up the strings Qt does not refresh on its own: QAction
    // texts and menu titles.
    retranslateToolbar();
    retranslateMenubar();
    if (themeMenu_) themeMenu_->retranslate();
    setThemeChecked();
    const bool zh = lang == AppTranslator::Chinese;
    if (langEnAct_) langEnAct_->setChecked(!zh);
    if (langZhAct_) langZhAct_->setChecked(zh);
    if (langMenubarEnAct_) langMenubarEnAct_->setChecked(!zh);
    if (langMenubarZhAct_) langMenubarZhAct_->setChecked(zh);
}

void MainWindow::closeTabAt(int idx) {
    if (!tabs_ || idx < 0 || idx >= tabs_->count()) return;
    auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(idx));
    if (!tab) return;
    std::shared_ptr<Document> doc = tab->editor()->document();
    dm_->closeDocument(doc);
    rc_->unbind(doc->path());
    if (!doc->path().isEmpty()) fileWatcher_->unwatch(doc->path());
    tabs_->removeTab(idx);
}

void MainWindow::updateEditActions() {
    QWidget* w = QApplication::focusWidget();
    const bool editable = qobject_cast<QPlainTextEdit*>(w) != nullptr;
    const bool browsable = qobject_cast<QWebEngineView*>(w) != nullptr;
    if (undoAct_)     undoAct_->setEnabled(editable || browsable);
    if (redoAct_)     redoAct_->setEnabled(editable || browsable);
    if (copyAct_)     copyAct_->setEnabled(editable || browsable);
    if (pasteAct_)    pasteAct_->setEnabled(editable || browsable);
    if (selectAllAct_)selectAllAct_->setEnabled(editable || browsable);
    // Cutting only makes sense where there is a text selection, so it
    // stays limited to the editor.
    if (cutAct_)     cutAct_->setEnabled(editable);
}

void MainWindow::newDocument() {
    auto doc = dm_->addNew();
    auto* tab = new DocumentTab(doc);
    rc_->bind(doc->path(), tab->preview(), tab);
    wireTabSync(tab);
    // Phase 12: paint the freshly-created editor with the active theme
    // so it doesn't flash white-on-default before the user touches the
    // theme menu.
    if (tab->editor()) tab->editor()->applyEditorPalette(
        mdreader::theme::paletteForTheme(theme_));
    tabs_->addTab(tab, "Untitled");
}

void MainWindow::wireTabSync(DocumentTab* tab) {
    if (!tab) return;
    // Outline click → move editor cursor to the heading's source line and
    // scroll the preview's matching <hN id="slug"> into view. The editor
    // and preview are siblings inside the tab; capture `tab` (not `this`)
    // to avoid dangling if MainWindow is destroyed before the tab.
    connect(tab->outline(), &OutlineView::headingActivated, tab,
            [tab](int line, const QString& slug) {
        auto* editor = tab->editor();
        auto* preview = tab->preview();
        if (!editor || !preview) return;
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        // line is 1-indexed; move down (line - 1) blocks. line <= 0 → no-op.
        for (int i = 0; i < line - 1; ++i) {
            cursor.movePosition(QTextCursor::NextBlock);
        }
        editor->setTextCursor(cursor);
        const QString js = QStringLiteral(
            "var el=document.getElementById('%1');"
            "if(el){el.scrollIntoView();}"
            ).arg(slug);
        preview->page()->runJavaScript(js);
    });

    // Editor cursor moved → highlight the last heading at-or-before the
    // current line in the outline. We re-extract on every cursor move
    // (cheap for typical doc sizes) so we always have up-to-date line
    // numbers without tracking edits ourselves.
    connect(tab->editor(), &QPlainTextEdit::cursorPositionChanged, tab,
            [tab]() {
        auto* editor = tab->editor();
        auto* outline = tab->outline();
        if (!editor || !outline) return;
        auto* model = outline->model();
        if (!model) return;
        const auto entries = OutlineExtractor::extract(editor->toPlainText());
        const int line = editor->textCursor().blockNumber() + 1;
        int matchIdx = -1;
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].lineNumber <= line) matchIdx = i;
            else break;
        }
        if (matchIdx >= 0) {
            const QModelIndex idx = model->index(matchIdx, 0);
            outline->setCurrentIndex(idx);
        }
    });
}

void MainWindow::openDocument() {
    auto path = QFileDialog::getOpenFileName(this, tr("Open MD"),
                                             QString(), tr("Markdown (*.md *.markdown)"));
    if (!path.isEmpty()) dm_->openFile(path);
}

void MainWindow::openFileFromCli(const QString& path) {
    if (path.isEmpty()) return;
    // Focus the existing tab if this path is already open — covers the
    // common case where the user double-clicks a .md that's already on
    // screen, or session-restore brings a file back while it's also still
    // on disk from a prior run that wasn't cleanly closed.
    if (auto* existing = tabForDocument_byPath(path)) {
        tabs_->setCurrentWidget(existing);
        return;
    }
    dm_->openFile(path);
}

bool MainWindow::saveCurrent() {
    auto* tab = currentTab();
    if (!tab) return false;
    auto doc = tab->editor()->document();
    if (!doc) return false;
    if (doc->path().isEmpty()) {
        auto path = QFileDialog::getSaveFileName(this, tr("Save MD"),
                                                 QString(), tr("Markdown (*.md)"));
        if (path.isEmpty()) return false;
        qWarning("MainWindow::saveCurrent: Save-As for new docs is a Task 24 stub "
                 "(user picked %s, ignoring)", qUtf8Printable(path));
        return false;
    }
    return dm_->saveDocument(doc);
}

void MainWindow::exportCurrentPdf() {
    auto* tab = currentTab();
    if (!tab) return;
    auto path = QFileDialog::getSaveFileName(this, tr("Export PDF"),
                                             QString(), tr("PDF (*.pdf)"));
    if (path.isEmpty()) return;
    tab->preview()->exportPdf(path);
}

void MainWindow::exportCurrentHtml() {
    auto* tab = currentTab();
    if (!tab) return;
    auto path = QFileDialog::getSaveFileName(this, tr("Export HTML"),
                                             QString(), tr("HTML (*.html *.htm)"));
    if (path.isEmpty()) return;
    // Qt 6 QWebEnginePage::toHtml is async (callback-based); use a local
    // event loop to fetch the current HTML synchronously for export.
    QString html;
    QEventLoop loop;
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    tab->preview()->page()->toHtml([&](const QString& h) {
        html = h;
        loop.quit();
    });
    loop.exec();
    if (html.isEmpty()) {
        qWarning("MainWindow::exportCurrentHtml: timed out or empty page");
        return;
    }
    tab->preview()->exportHtml(path, html);
}

void MainWindow::toggleOutline() { outlineVisible_ = outlineAct_->isChecked(); }
void MainWindow::toggleInfo()    { infoVisible_    = infoAct_->isChecked(); }

void MainWindow::onTabChanged(int) {
    auto* tab = currentTab();
    if (!tab) return;
    tab->outline()->setVisible(outlineVisible_);
    tab->info()->setVisible(infoVisible_);
    // Phase 12: when the user switches tabs while the find bar is open,
    // close it. The bar was bound to the previous editor's cursor /
    // selection; showing it now would search stale text and almost
    // certainly surprise the user. They'll press Ctrl+F again on the new
    // tab and get a fresh search seeded from the new selection.
    if (findBar_ && findBar_->isVisibleBar()) {
        findBar_->hide();
        emit findBar_->findBarClosed();
    }
}

DocumentTab* MainWindow::currentTab() const {
    return qobject_cast<DocumentTab*>(tabs_->currentWidget());
}

DocumentTab* MainWindow::tabForDocument_byPath(const QString& path) const {
    if (!tabs_) return nullptr;
    for (int i = 0; i < tabs_->count(); ++i) {
        auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(i));
        if (!tab) continue;
        auto doc = tab->editor()->document();
        if (doc && doc->path() == path) return tab;
    }
    return nullptr;
}

void MainWindow::onExternalChange(const QString& path, const QByteArray& bytes) {
    auto* tab = tabForDocument_byPath(path);
    if (!tab) return;  // tab was closed between FS event and this callback
    auto doc = tab->editor()->document();
    if (!doc) return;
    if (bytes.isEmpty()) {
        qWarning("MainWindow::onExternalChange: empty bytes for %s (file may have been deleted or unreadable)",
                 qUtf8Printable(path));
        return;
    }
    const QString disk = QString::fromUtf8(bytes);

    // Shared "apply the on-disk content" path used by both the silent
    // no-conflict branch and the user-confirmed discard/backup branches.
    // Document::setText marks dirty — acceptable here because the next
    // save (if any) will overwrite disk with this new content anyway.
    // We do not call markSaved() because that would lie about whether
    // the user has actually persisted the disk content themselves.
    auto reloadFromDisk = [this, tab, doc, disk]() {
        doc->setText(disk);
        tab->editor()->setTextDirect(disk);
        rc_->requestRender(doc->path(), disk, theme_, /*force=*/true);
    };

    if (!doc->dirty() || doc->text() == disk) {
        // Either truly clean OR the doc already mirrors disk (race window
        // between our last save and the FS event). Silent reload.
        reloadFromDisk();
        statusBar()->showMessage(tr("Reloaded from disk"), 3000);
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setText(tr("File changed on disk: %1").arg(QFileInfo(path).fileName()));
    box.setInformativeText(tr("How do you want to handle it?"));
    QPushButton* discard = box.addButton(tr("Discard my edits and reload"),
                                         QMessageBox::AcceptRole);
    QPushButton* keep    = box.addButton(tr("Keep my edits"),
                                         QMessageBox::RejectRole);
    QPushButton* backup  = box.addButton(tr("Backup then reload"),
                                         QMessageBox::ActionRole);
    box.setDefaultButton(discard);
    box.exec();
    QAbstractButton* clicked = box.clickedButton();
    if (clicked == discard) {
        reloadFromDisk();
    } else if (clicked == backup) {
        const QString stamp = QDateTime::currentDateTimeUtc()
                                  .toString(QStringLiteral("yyyyMMdd-HHmmss"));
        const QString bak = path + QStringLiteral(".conflict-") + stamp
                            + QStringLiteral(".md");
        QFile b(bak);
        bool ok = false;
        if (b.open(QIODevice::WriteOnly)) {
            QByteArray bytes = doc->text().toUtf8();
            qint64 wrote = b.write(bytes);
            b.close();
            ok = (wrote == bytes.size());
            if (!ok) {
                qWarning("MainWindow::onExternalChange: backup write short at %s (wrote %lld of %lld)",
                         qUtf8Printable(bak), (long long)wrote, (long long)bytes.size());
            }
        }
        if (!ok) {
            statusBar()->showMessage(tr("Backup failed for %1").arg(bak), 5000);
            return;
        }
        reloadFromDisk();
        statusBar()->showMessage(tr("Saved backup to %1").arg(bak), 5000);
    } else {
        // "Keep my edits" (or dialog dismissed) — stop watching so the
        // user is not pestered again. They can still re-watch by reopening.
        fileWatcher_->unwatch(path);
    }
}

void MainWindow::loadSettings() {
    QSettings s;
    lastFiles_       = s.value(kKeyLastFiles, QStringList()).toStringList();
    currentIndex_    = s.value(kKeyCurrentIndex, -1).toInt();
    // Phase 7: themeMode_ persists whether the user wants to follow the OS
    // color scheme; userTheme_ is the concrete theme they last picked (used
    // when themeMode_ == "manual"). Older settings only wrote kKeyTheme —
    // we treat that as "manual" + userTheme_ = saved value, which preserves
    // the pre-Phase-7 experience.
    themeMode_       = s.value(kKeyThemeMode, kValueThemeManual).toString();
    userTheme_       = s.value(kKeyTheme,     kAutoLightTheme).toString();
    theme_           = resolveEffectiveTheme();
    renderMode_      = s.value(kKeyRenderMode, kValueLive).toString();
    showLineNumbers_ = s.value(kKeyShowLineNumbers, true).toBool();
    splitterA_state_ = s.value(kKeySplitterA).toByteArray();
    splitterB_state_ = s.value(kKeySplitterB).toByteArray();
    splitterC_state_ = s.value(kKeySplitterC).toByteArray();
    outlineVisible_  = s.value(kKeyOutlineVisible, true).toBool();
    infoVisible_     = s.value(kKeyInfoVisible,    true).toBool();

    for (const auto& f : lastFiles_) {
        if (QFile::exists(f)) dm_->openFile(f);
    }

    // Tell the theme menus which entry to tick: "auto" selects Follow
    // System, anything else selects the matching concrete theme. The
    // helper covers both the toolbar dropdown and the View ▸ Theme
    // submenu in the menu bar.
    setThemeChecked();

    manualAct_->setChecked(renderMode_ == kValueManual);
    liveAct_->setChecked(renderMode_ != kValueManual);
    refreshAct_->setEnabled(renderMode_ == kValueManual);
}

void MainWindow::saveSettings() {
    QSettings s;
    lastFiles_.clear();
    for (int i = 0; i < tabs_->count(); ++i) {
        auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(i));
        if (tab) lastFiles_ << tab->editor()->document()->path();
    }
    s.setValue(kKeyLastFiles, lastFiles_);
    s.setValue(kKeyCurrentIndex, tabs_->currentIndex());
    s.setValue(kKeyThemeMode, themeMode_);
    s.setValue(kKeyTheme,     userTheme_);
    s.setValue(kKeyRenderMode, renderMode_);
    s.setValue(kKeyShowLineNumbers, showLineNumbers_);
    if (auto* tab = currentTab()) {
        s.setValue(kKeySplitterA, tab->splitterA()->saveState());
        s.setValue(kKeySplitterB, tab->splitterB()->saveState());
        s.setValue(kKeySplitterC, tab->splitterC()->saveState());
    }
    s.setValue(kKeyOutlineVisible, outlineVisible_);
    s.setValue(kKeyInfoVisible,    infoVisible_);
    // On macOS QSettings writes a plist and only flushes it when the
    // QSettings object is destroyed — which does not happen if the app is
    // killed. sync() makes sure the session is on disk right away.
    s.sync();
}

QString MainWindow::resolveEffectiveTheme() const {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    using mdreader::theme::SystemColorScheme;
    const SystemColorScheme systemScheme = static_cast<SystemColorScheme>(
        static_cast<int>(QGuiApplication::styleHints()->colorScheme()));
#else
    // Qt < 6.5 has no colorScheme() — we have no way to read the OS scheme,
    // so resolveTheme() will be called with Unknown and fall into the
    // light branch (kAutoLightTheme). Follow-system is effectively disabled.
    using mdreader::theme::SystemColorScheme;
    const SystemColorScheme systemScheme = SystemColorScheme::Unknown;
#endif
    return mdreader::theme::resolveTheme(
        themeMode_, userTheme_, systemScheme);
}

void MainWindow::rerenderAllTabs(const QString& effectiveTheme) {
    for (int i = 0; i < tabs_->count(); ++i) {
        auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(i));
        if (!tab) continue;
        auto doc = tab->editor()->document();
        if (!doc) continue;
        rc_->requestRender(doc->path(), doc->text(), effectiveTheme,
                           /*force=*/true);
    }
}

void MainWindow::onSystemColorSchemeChanged(Qt::ColorScheme /*scheme*/) {
    // No-op when the user has pinned a specific theme — their choice wins.
    if (themeMode_ != kModeAuto) return;
    theme_ = resolveEffectiveTheme();
    rerenderAllTabs(theme_);
    // Phase 12: editor follows the OS flip too.
    applyEditorTheme();
}

void MainWindow::applyEditorTheme() {
    const auto palette = mdreader::theme::paletteForTheme(theme_);
    for (int i = 0; i < tabs_->count(); ++i) {
        auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(i));
        if (!tab) continue;
        if (auto* editor = tab->editor()) {
            editor->applyEditorPalette(palette);
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    // Accept the proposed action iff the payload carries at least one
    // local file. The full extension / existence filter happens in
    // dropEvent — dragEnter just needs to say "yes the user is dragging
    // files at us" so the OS shows the copy cursor.
    if (mdreader::drop::canAccept(e->mimeData())) {
        e->acceptProposedAction();
    } else {
        e->ignore();
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent* e) {
    // Same gate as dragEnter — without it the cursor reverts to "no
    // drop" mid-flight.
    if (mdreader::drop::canAccept(e->mimeData())) {
        e->acceptProposedAction();
    } else {
        e->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* e) {
    const QStringList paths = mdreader::drop::extractLocalMarkdownPaths(e->mimeData());
    if (paths.isEmpty()) {
        e->ignore();
        return;
    }
    // Forward each path to DocumentManager; it dispatches the read to a
    // worker thread and emits documentLoaded when done, which we wire up
    // in the ctor to create a tab per file.
    for (const QString& p : paths) {
        dm_->openFile(p);
    }
    e->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveSettings();
    QMainWindow::closeEvent(e);
}

void MainWindow::retranslateToolbar() {
    // Walk every action on the main toolbar and re-set its text from the
    // current tr() mapping. Each toolbar action carries the original
    // English source literal in QAction::data() (set in buildUi()), so
    // we can re-translate without losing context. We do this instead of
    // relying on Qt's automatic LanguageChange handling because
    // QAction::setText isn't called automatically on translator swap —
    // only widgets like QPushButton / QLabel re-run their tr() calls.
    const auto toolbars = findChildren<QToolBar*>();
    for (QToolBar* tb : toolbars) {
        for (QAction* a : tb->actions()) {
            const QString src = a->data().toString();
            if (src.isEmpty()) continue;
            // Convert back to const char* so tr() picks the right entry
            // from the active translator (matching the original literal
            // byte-for-byte).
            a->setText(tr(src.toUtf8().constData()));
        }
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* e) {
    // Phase 9: double-click on the toolbar's empty area → toggle fullscreen.
    // We only react when:
    //   1. The watched object is a QToolBar (we installed the filter on `tb`
    //      in buildUi()).
    //   2. The event is a left-button double-click.
    //   3. The dbl-click landed on the toolbar itself OR on a non-button
    //      child (separator, layout stretch). If the target is a
    //      QToolButton we return false so its normal click handler still
    //      runs (we don't want double-clicking "Refresh" to fullscreen
    //      instead of triggering the action).
    if (e->type() == QEvent::MouseButtonDblClick) {
        auto* mbe = static_cast<QMouseEvent*>(e);
        if (mbe->button() == Qt::LeftButton && qobject_cast<QToolBar*>(watched)) {
            // Walk up to find what's actually under the cursor. The event
            // arrives with `watched` as the toolbar, but childAt() inside
            // the toolbar tells us whether the user clicked on a button or
            // on the empty area.
            auto* tb = static_cast<QToolBar*>(watched);
            QPoint p = mbe->position().toPoint();
            // For events delivered to the toolbar itself, position() is
            // already in toolbar-local coords. For child events it would
            // be in the child's coords; the cast above keeps it simple
            // because we filter at the toolbar level.
            QWidget* child = tb->childAt(p);
            if (!child || !qobject_cast<QToolButton*>(child)) {
                if (windowState() & Qt::WindowFullScreen) {
                    setWindowState(windowState() & ~Qt::WindowFullScreen);
                } else {
                    setWindowState(windowState() | Qt::WindowFullScreen);
                }
                return true;  // consume so it doesn't bubble
            }
            // Else: a button was dbl-clicked — let it through.
        }
    }
    return QMainWindow::eventFilter(watched, e);
}

void MainWindow::changeEvent(QEvent* e) {
    // Phase 13: redirect WM-driven maximize events into WindowFullScreen.
    //
    // The native title bar is owned by the window manager, so we never see
    // the double-click itself. What we do see is the resulting state
    // change: WM maximizes the window → Qt fires WindowStateChange with
    // oldState() = the prior state. We use the helper to decide what state
    // to apply (see app/TitleBarFullscreen.h for the rules and rationale).
    if (e->type() == QEvent::WindowStateChange) {
        auto* wse = static_cast<QWindowStateChangeEvent*>(e);
        const Qt::WindowStates now = windowState();
        const Qt::WindowStates old = wse->oldState();

        // Phase 15 (macOS): leaving fullscreen. macOS restores the
        // window's zoomed frame a moment later and Qt reports *that* as a
        // plain maximize transition (old state = NoState) — the same
        // shape as the title-bar double-click we redirect into fullscreen.
        // Redirecting it would shrink the window for a frame and snap it
        // straight back, so remember that we are on our way out and let
        // exactly one maximize transition through untouched.
        if ((old & Qt::WindowFullScreen) && !(now & Qt::WindowFullScreen)) {
            suppressMaximizeRedirect_ = true;
            // Safety net: if the zoom restore never arrives (it doesn't on
            // every platform), don't suppress a later legitimate maximize
            // forever.
            QTimer::singleShot(1000, this, [this] {
                suppressMaximizeRedirect_ = false;
            });
        }

        Qt::WindowStates target =
            mdreader::window::redirectTitleBarMaximize(now, old);
        if (suppressMaximizeRedirect_) {
            target = now;
            if (now & Qt::WindowMaximized) {
                // That was the zoom restore macOS performs on the way out
                // of fullscreen — consumed, later maximizes redirect again.
                suppressMaximizeRedirect_ = false;
            }
        }
        if (target != windowState()) {
            // setWindowState fires WindowStateChange a second time. On
            // that re-entry the helper returns `target` unchanged
            // (idempotent — the enteringMaximize flag is false because
            // the redirected state already carries WindowFullScreen),
            // so the recursive call is a no-op and we don't loop.
            setWindowState(target);
        }
        // Phase 14 (macOS): keep the View ▸ Full Screen tick in sync when
        // the fullscreen state was changed from somewhere else (title-bar
        // double-click, Esc).
        if (fullscreenAct_) {
            fullscreenAct_->setChecked(windowState() & Qt::WindowFullScreen);
        }
        // Consume the event so the base class doesn't try to apply our
        // already-handled target a second time (harmless but wasteful).
        return;
    }
    QMainWindow::changeEvent(e);
}

bool MainWindow::event(QEvent* e) {
    // Phase 14 (macOS): when MDReader is already running and the user
    // double-clicks a .md file in Finder, macOS does not start a second
    // process and does not pass the path in argv — it asks the running
    // application to open the file by sending an Apple Event, which Qt
    // turns into a QFileOpenEvent. QApplication forwards that event to
    // the top-level widgets, so handling it here is what makes "Open
    // With ▸ MDReader" work on an already-running instance.
    if (e->type() == QEvent::FileOpen) {
        auto* foe = static_cast<QFileOpenEvent*>(e);
        QString path = foe->file();
        if (path.isEmpty() && foe->url().isLocalFile()) {
            path = foe->url().toLocalFile();
        }
        if (!path.isEmpty()) {
            openFileFromCli(path);
            return true;
        }
    }
    return QMainWindow::event(e);
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    // Phase 13: Esc exits fullscreen. We only intercept Esc when we're
    // actually in fullscreen — otherwise we'd swallow the user's Esc in
    // the editor (e.g. closing an autocompletion popup, dismissing the
    // FindBar's own shortcut). The FindBar handles its own widget-scoped
    // Esc shortcut, so when the bar is visible Esc reaches it first;
    // when the bar is hidden Esc propagates up to us.
    if (e->key() == Qt::Key_Escape
        && (windowState() & Qt::WindowFullScreen)) {
        setWindowState(windowState() & ~Qt::WindowFullScreen);
        e->accept();
        return;
    }
    QMainWindow::keyPressEvent(e);
}