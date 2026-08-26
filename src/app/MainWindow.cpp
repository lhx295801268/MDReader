#include "app/MainWindow.h"
#include "documents/Document.h"
#include "documents/DocumentManager.h"
#include "ui/DocumentTab.h"
#include "ui/EditorView.h"
#include "ui/PreviewView.h"
#include "ui/OutlineView.h"
#include "ui/InfoView.h"
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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MDReader");
    resize(1280, 800);
    dm_ = new DocumentManager(this);
    rc_ = new RenderCoordinator(this);
    buildUi();
    loadSettings();

    connect(dm_, &DocumentManager::documentLoaded, this,
            [this](std::shared_ptr<Document> doc) {
        auto* tab = new DocumentTab(doc);
        rc_->bind(doc->path(), tab->preview());
        tabs_->addTab(tab, QFileInfo(doc->path()).fileName());
        if (lastFiles_.size() > 0 && currentIndex_ < tabs_->count()) {
            tabs_->setCurrentIndex(currentIndex_);
        }
        currentIndex_ = tabs_->currentIndex();
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

    connect(newAct, &QAction::triggered, this, &MainWindow::newDocument);
    connect(openAct, &QAction::triggered, this, &MainWindow::openDocument);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveCurrent);
    connect(pdfAct, &QAction::triggered, this, &MainWindow::exportCurrentPdf);
    connect(htmlAct, &QAction::triggered, this, &MainWindow::exportCurrentHtml);
    connect(outlineAct_, &QAction::toggled, this, &MainWindow::toggleOutline);
    connect(infoAct_, &QAction::toggled, this, &MainWindow::toggleInfo);

    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(dm_, &DocumentManager::documentAdded,
            this, &MainWindow::onDocumentAdded);
}

void MainWindow::newDocument() {
    auto doc = dm_->addNew();
    auto* tab = new DocumentTab(doc);
    rc_->bind(doc->path(), tab->preview());
    tabs_->addTab(tab, "Untitled");
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
        return false;  // SaveAs flow not in this task; see Task 24.
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
    tab->preview()->page()->toHtml([&](const QString& h) {
        html = h;
        loop.quit();
    });
    loop.exec();
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

void MainWindow::onDocumentAdded(std::shared_ptr<Document>) { /* no-op */ }
void MainWindow::onDocumentClosed(std::shared_ptr<Document>) { /* nothing extra */ }

DocumentTab* MainWindow::currentTab() const {
    return qobject_cast<DocumentTab*>(tabs_->currentWidget());
}

DocumentTab* MainWindow::tabForDocument(std::shared_ptr<Document>) const {
    return nullptr;
}

void MainWindow::loadSettings() {
    QSettings s;
    lastFiles_       = s.value("session/lastFiles", QStringList()).toStringList();
    currentIndex_    = s.value("session/currentIndex", -1).toInt();
    theme_           = s.value("ui/theme", "github").toString();
    renderMode_      = s.value("ui/renderMode", "live").toString();
    showLineNumbers_ = s.value("ui/showLineNumbers", true).toBool();
    splitterA_state_ = s.value("layout/splitterA").toByteArray();
    splitterB_state_ = s.value("layout/splitterB").toByteArray();
    splitterC_state_ = s.value("layout/splitterC").toByteArray();
    outlineVisible_  = s.value("sidebar/outlineVisible", true).toBool();
    infoVisible_     = s.value("sidebar/infoVisible",    true).toBool();

    for (const auto& f : lastFiles_) {
        if (QFile::exists(f)) dm_->openFile(f);
    }
}

void MainWindow::saveSettings() {
    QSettings s;
    lastFiles_.clear();
    for (int i = 0; i < tabs_->count(); ++i) {
        auto* tab = qobject_cast<DocumentTab*>(tabs_->widget(i));
        if (tab) lastFiles_ << tab->editor()->document()->path();
    }
    s.setValue("session/lastFiles", lastFiles_);
    s.setValue("session/currentIndex", tabs_->currentIndex());
    s.setValue("ui/theme", theme_);
    s.setValue("ui/renderMode", renderMode_);
    s.setValue("ui/showLineNumbers", showLineNumbers_);
    if (auto* tab = currentTab()) {
        s.setValue("layout/splitterA", tab->splitterA()->saveState());
        s.setValue("layout/splitterB", tab->splitterB()->saveState());
        s.setValue("layout/splitterC", tab->splitterC()->saveState());
    }
    s.setValue("sidebar/outlineVisible", outlineVisible_);
    s.setValue("sidebar/infoVisible",    infoVisible_);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveSettings();
    QMainWindow::closeEvent(e);
}