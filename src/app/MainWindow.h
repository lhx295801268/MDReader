#pragma once
#include <QMainWindow>
#include <QStringList>
#include <QDateTime>
#include <QMessageBox>
#include <memory>
#include "services/FileWatcher.h"

class QTabWidget;
class DocumentManager;
class DocumentTab;
class RenderCoordinator;
class ThemeMenu;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void openFileFromCli(const QString& path);

protected:
    void closeEvent(QCloseEvent* e) override;

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

private:
    void buildUi();
    void loadSettings();
    void saveSettings();

    // Task 20: connect outline ↔ editor ↔ preview bidirectional sync for a
    // newly-created tab. Called by both documentLoaded and newDocument.
    void wireTabSync(DocumentTab* tab);

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
    QString theme_ = "github";
    QString renderMode_ = "live";
    bool showLineNumbers_ = true;
    QAction* outlineAct_ = nullptr;
    QAction* infoAct_ = nullptr;
    QAction* liveAct_ = nullptr;
    QAction* manualAct_ = nullptr;
    QAction* refreshAct_ = nullptr;
    ThemeMenu* themeMenu_ = nullptr;
    QByteArray splitterA_state_;
    QByteArray splitterB_state_;
    QByteArray splitterC_state_;
};