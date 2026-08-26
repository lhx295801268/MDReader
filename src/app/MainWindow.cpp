#include "app/MainWindow.h"
#include "documents/Document.h"
#include "documents/DocumentManager.h"
#include "ui/DocumentTab.h"
#include "ui/EditorView.h"
#include "ui/PreviewView.h"
#include "ui/OutlineView.h"
#include "ui/InfoView.h"
#include "ui/ThemeMenu.h"
#include "services/RenderCoordinator.h"
#include "services/WordCounter.h"
#include "services/OutlineExtractor.h"
#include <QApplication>
#include <QTabWidget>
#include <QAction>
#include <QToolBar>
#include <QFileDialog>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QPointer>
#include <QActionGroup>
#include <QTextCursor>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include "services/FileWatcher.h"

namespace {
constexpr auto kKeyLastFiles       = "session/lastFiles";
constexpr auto kKeyCurrentIndex    = "session/currentIndex";
constexpr auto kKeyTheme           = "ui/theme";
constexpr auto kKeyRenderMode      = "ui/renderMode";
constexpr auto kKeyShowLineNumbers = "ui/showLineNumbers";
constexpr auto kKeySplitterA       = "layout/splitterA";
constexpr auto kKeySplitterB       = "layout/splitterB";
constexpr auto kKeySplitterC       = "layout/splitterC";
constexpr auto kKeyOutlineVisible  = "sidebar/outlineVisible";
constexpr auto kKeyInfoVisible     = "sidebar/infoVisible";
constexpr auto kValueLive          = "live";
constexpr auto kValueManual        = "manual";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MDReader");
    resize(1280, 800);
    dm_ = new DocumentManager(this);
    rc_ = new RenderCoordinator(this);
    // Phase 4: external-change notifier. Watches each opened document's
    // path; emits externalModified(path, bytes) after debounce when the file
    // is rewritten on disk by another process. We react in onExternalChange.
    fileWatcher_ = new FileWatcher(this);
    connect(fileWatcher_, &FileWatcher::externalModified,
            this, &MainWindow::onExternalChange);
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
        auto* tab = new DocumentTab(doc);
        // Task 20: pass the tab so RenderCoordinator can dispatch outline +
        // stats back to it after each render.
        self->rc_->bind(doc->path(), tab->preview(), tab);
        // Outline click → editor cursor + preview scroll;
        // editor cursor → outline highlight.
        self->wireTabSync(tab);
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
    connect(tabs_, &QTabWidget::tabCloseRequested, this, [this](int idx) {
        auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(idx));
        if (!tab) return;
        std::shared_ptr<Document> doc = tab->editor()->document();
        dm_->closeDocument(doc);
        rc_->unbind(doc->path());
        if (!doc->path().isEmpty()) fileWatcher_->unwatch(doc->path());
        tabs_->removeTab(idx);
    });
    setCentralWidget(tabs_);

    auto* tb = addToolBar("Main");
    auto* newAct = tb->addAction("New");
    auto* openAct = tb->addAction("Open");
    auto* saveAct = tb->addAction("Save");
    tb->addSeparator();
    auto* pdfAct = tb->addAction("Export PDF");
    auto* htmlAct = tb->addAction("Export HTML");
    tb->addSeparator();
    outlineAct_ = tb->addAction("Outline");
    outlineAct_->setCheckable(true); outlineAct_->setChecked(true);
    infoAct_ = tb->addAction("Info");
    infoAct_->setCheckable(true); infoAct_->setChecked(true);

    themeMenu_ = new ThemeMenu(this);
    auto* themeBtn = tb->addAction("Theme");
    themeBtn->setMenu(themeMenu_);
    connect(themeMenu_, &ThemeMenu::themeSelected, this, [this](QString t) {
        theme_ = t;
        for (int i = 0; i < tabs_->count(); ++i) {
            auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(i));
            if (!tab) continue;
            auto doc = tab->editor()->document();
            if (!doc) continue;
            rc_->requestRender(doc->path(), doc->text(), t);
        }
    });

    tb->addSeparator();
    liveAct_ = tb->addAction("Live");
    liveAct_->setCheckable(true); liveAct_->setChecked(true);
    manualAct_ = tb->addAction("Manual");
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

    refreshAct_ = tb->addAction("Refresh");
    refreshAct_->setEnabled(false);  // disabled while in Live mode (no Refresh needed)
    connect(refreshAct_, &QAction::triggered, this, [this] {
        auto* tab = currentTab();
        if (!tab) return;
        auto doc = tab->editor()->document();
        if (!doc) return;
        rc_->requestRender(doc->path(), doc->text(), theme_, /*force=*/true);
    });

    connect(newAct, &QAction::triggered, this, &MainWindow::newDocument);
    connect(openAct, &QAction::triggered, this, &MainWindow::openDocument);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveCurrent);
    connect(pdfAct, &QAction::triggered, this, &MainWindow::exportCurrentPdf);
    connect(htmlAct, &QAction::triggered, this, &MainWindow::exportCurrentHtml);
    connect(outlineAct_, &QAction::toggled, this, &MainWindow::toggleOutline);
    connect(infoAct_, &QAction::toggled, this, &MainWindow::toggleInfo);

    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
}

void MainWindow::newDocument() {
    auto doc = dm_->addNew();
    auto* tab = new DocumentTab(doc);
    rc_->bind(doc->path(), tab->preview(), tab);
    wireTabSync(tab);
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
    auto path = QFileDialog::getOpenFileName(this, "Open MD",
                                             QString(), "Markdown (*.md *.markdown)");
    if (!path.isEmpty()) dm_->openFile(path);
}

void MainWindow::openFileFromCli(const QString& path) {
    if (!path.isEmpty()) dm_->openFile(path);
}

bool MainWindow::saveCurrent() {
    auto* tab = currentTab();
    if (!tab) return false;
    auto doc = tab->editor()->document();
    if (!doc) return false;
    if (doc->path().isEmpty()) {
        auto path = QFileDialog::getSaveFileName(this, "Save MD",
                                                 QString(), "Markdown (*.md)");
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
    auto path = QFileDialog::getSaveFileName(this, "Export PDF",
                                             QString(), "PDF (*.pdf)");
    if (path.isEmpty()) return;
    tab->preview()->exportPdf(path);
}

void MainWindow::exportCurrentHtml() {
    auto* tab = currentTab();
    if (!tab) return;
    auto path = QFileDialog::getSaveFileName(this, "Export HTML",
                                             QString(), "HTML (*.html *.htm)");
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
    theme_           = s.value(kKeyTheme, "github").toString();
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

    themeMenu_->setCurrent(theme_);

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
    s.setValue(kKeyTheme, theme_);
    s.setValue(kKeyRenderMode, renderMode_);
    s.setValue(kKeyShowLineNumbers, showLineNumbers_);
    if (auto* tab = currentTab()) {
        s.setValue(kKeySplitterA, tab->splitterA()->saveState());
        s.setValue(kKeySplitterB, tab->splitterB()->saveState());
        s.setValue(kKeySplitterC, tab->splitterC()->saveState());
    }
    s.setValue(kKeyOutlineVisible, outlineVisible_);
    s.setValue(kKeyInfoVisible,    infoVisible_);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveSettings();
    QMainWindow::closeEvent(e);
}