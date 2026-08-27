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
namespace Qt { enum class ColorScheme; }

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
    ThemeMenu* themeMenu_ = nullptr;
    QByteArray splitterA_state_;
    QByteArray splitterB_state_;
    QByteArray splitterC_state_;
};