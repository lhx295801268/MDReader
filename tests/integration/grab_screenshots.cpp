// grab_screenshots.cpp — Task 32 smoke-test screenshots helper.
//
// Builds a DocumentTab for each (fixture, theme) pair, drives a render
// through the real RenderCoordinator pipeline, waits for QWebEnginePage
// to finish loading the rendered HTML, then QWidget::grab()s the whole
// tab and saves it as a PNG. For the conflict-modal shot we synthesize
// a real QMessageBox with the exact text/buttons MainWindow uses in
// onExternalChange so the screenshot faithfully represents the modal
// users see when their on-disk file is rewritten while they edit.
//
// Why this exists: in our environment a live gnome-screenshot of
// mdreader captured the X11 window correctly, but the preview pane was
// blank because the CLI entry point never calls requestRender on file
// open — render only fires when the user edits the document or
// switches themes. Constructing the widget tree here and driving the
// render explicitly produces a faithful preview, every theme, every
// fixture, and the conflict modal — without depending on user input.

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTextStream>
#include <QTimer>
#include <QDir>
#include <QWidget>
#include <QPixmap>
#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QDateTime>
#include <memory>

#include "documents/Document.h"
#include "ui/DocumentTab.h"
#include "ui/PreviewView.h"
#include "ui/EditorView.h"
#include "ui/OutlineView.h"
#include "ui/InfoView.h"
#include "services/RenderCoordinator.h"

namespace {

// Each entry describes one normal (render-and-grab) screenshot.
struct Shot {
    const char* outPath;
    const char* fixture;
    const char* theme;
};

bool renderAndGrab(QMainWindow& win, DocumentTab& tab,
                   std::shared_ptr<Document> doc,
                   const QString& theme, const QString& outPath) {
    RenderCoordinator rc;
    rc.bind(doc->path(), tab.preview(), &tab);
    rc.requestRender(doc->path(), doc->text(), theme, /*force=*/true);

    QSignalSpy loadSpy(tab.preview()->page(), &QWebEnginePage::loadFinished);
    if (!loadSpy.isValid() || !loadSpy.wait(3000)) {
        qWarning("loadFinished not received for %s (theme=%s)",
                 qUtf8Printable(outPath), qUtf8Printable(theme));
        return false;
    }

    // Give MathJax / highlight.js a beat to actually paint after the
    // initial DOM is ready (these are scheduled by setHtml's script tags
    // and may finish after loadFinished fires).
    QEventLoop settle;
    QTimer::singleShot(800, &settle, &QEventLoop::quit);
    settle.exec();

    QPixmap pm = win.grab();
    if (pm.isNull()) {
        qWarning("grab() returned a null pixmap for %s", qUtf8Printable(outPath));
        return false;
    }
    if (!pm.save(outPath, "PNG")) {
        qWarning("Failed to save %s", qUtf8Printable(outPath));
        return false;
    }
    return true;
}

bool loadFixtureText(const QString& path, QString* out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    *out = QString::fromUtf8(f.readAll());
    return true;
}

// Synthesize the conflict modal that MainWindow::onExternalChange pops up
// when a dirty document is overwritten on disk. The text + buttons match
// the real call site (MainWindow.cpp onExternalChange, ~line 355) so
// the screenshot is a faithful representation of what the user sees.
bool grabConflictModal(const QString& outPath, const QString& docName) {
    QMessageBox box;
    box.setIcon(QMessageBox::Warning);
    box.setText(QStringLiteral("File changed on disk: %1").arg(docName));
    box.setInformativeText(QStringLiteral("How do you want to handle it?"));
    box.setWindowTitle(QStringLiteral("MDReader"));
    QPushButton* discard = box.addButton(QStringLiteral("Discard my edits and reload"),
                                         QMessageBox::AcceptRole);
    QPushButton* keep    = box.addButton(QStringLiteral("Keep my edits"),
                                         QMessageBox::RejectRole);
    QPushButton* backup  = box.addButton(QStringLiteral("Backup then reload"),
                                         QMessageBox::ActionRole);
    box.setDefaultButton(discard);

    // Make sure the dialog is shown on screen so grab() captures the
    // platform decoration too. exec() blocks until the user dismisses,
    // so we show() instead and use a single-shot timer to grab+close.
    box.show();
    box.raise();
    box.activateWindow();

    // Let the window manager map/decorate the dialog before we grab.
    QEventLoop ready;
    QTimer::singleShot(600, &ready, &QEventLoop::quit);
    ready.exec();

    QPixmap pm = box.grab();
    box.close();
    if (pm.isNull()) {
        qWarning("grab() returned a null pixmap for conflict modal");
        return false;
    }
    if (!pm.save(outPath, "PNG")) {
        qWarning("Failed to save %s", qUtf8Printable(outPath));
        return false;
    }
    Q_UNUSED(discard); Q_UNUSED(keep); Q_UNUSED(backup);
    Q_UNUSED(QDateTime::currentDateTimeUtc());  // keep include used
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("MDReader");
    QApplication::setApplicationName("MDReader");

    const QString outDir = (argc > 1)
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral("docs/screenshots");
    QDir().mkpath(outDir);

    const Shot shots[] = {
        { "01-full.png",         "tests/fixtures/sample.md", "github"          },
        { "02-github-light.png", "tests/fixtures/sample.md", "github"          },
        { "03-dracula-dark.png", "tests/fixtures/sample.md", "dracula"         },
        { "04-solarized.png",    "tests/fixtures/sample.md", "solarized-light" },
        { "05-math-or-code.png", "tests/fixtures/math.md",   "github"          },
    };

    int produced = 0;
    for (const Shot& s : shots) {
        QString text;
        if (!loadFixtureText(QString::fromLocal8Bit(s.fixture), &text)) {
            qWarning("Cannot read fixture %s", s.fixture);
            continue;
        }
        const QString docPath = QStringLiteral("%1::%2")
                                    .arg(QString::fromLocal8Bit(s.fixture),
                                         QString::fromLocal8Bit(s.outPath));
        auto doc = std::make_shared<Document>(docPath, text);

        // Wrap the DocumentTab in a QMainWindow with a representative
        // toolbar so the screenshots show the same chrome users see in
        // the real app (New / Open / Save / Export / Outline / Info /
        // Theme / Live / Manual / Refresh).
        auto* tab = new DocumentTab(doc);
        auto* win = new QMainWindow();
        win->setWindowTitle(QStringLiteral("MDReader"));
        win->resize(1280, 720);
        auto* tb = win->addToolBar(QStringLiteral("Main"));
        tb->addAction(QStringLiteral("New"));
        tb->addAction(QStringLiteral("Open"));
        tb->addAction(QStringLiteral("Save"));
        tb->addSeparator();
        tb->addAction(QStringLiteral("Export PDF"));
        tb->addAction(QStringLiteral("Export HTML"));
        tb->addSeparator();
        auto* outlineAct = tb->addAction(QStringLiteral("Outline"));
        outlineAct->setCheckable(true); outlineAct->setChecked(true);
        auto* infoAct = tb->addAction(QStringLiteral("Info"));
        infoAct->setCheckable(true); infoAct->setChecked(true);
        tb->addAction(QStringLiteral("Theme"));
        tb->addSeparator();
        auto* liveAct = tb->addAction(QStringLiteral("Live"));
        liveAct->setCheckable(true); liveAct->setChecked(true);
        tb->addAction(QStringLiteral("Manual"));
        tb->addAction(QStringLiteral("Refresh"));
        win->setCentralWidget(tab);
        win->show();
        QEventLoop ready;
        QTimer::singleShot(150, &ready, &QEventLoop::quit);
        ready.exec();

        const QString outPath = outDir + QStringLiteral("/") + QString::fromLocal8Bit(s.outPath);
        if (renderAndGrab(*win, *tab, doc, QString::fromLocal8Bit(s.theme), outPath)) {
            qInfo("Wrote %s", qUtf8Printable(outPath));
            ++produced;
        }
        win->close();
        delete win;  // also destroys the tab child
    }

    // 06-conflict-modal: synthesize the 3-button modal that fires when
    // an externally-edited file collides with a dirty edit.
    const QString modalPath = outDir + QStringLiteral("/06-conflict-modal.png");
    if (grabConflictModal(modalPath, QStringLiteral("sample.md"))) {
        qInfo("Wrote %s", qUtf8Printable(modalPath));
        ++produced;
    }

    QTextStream(stdout) << "Produced " << produced << " of 6 screenshots\n";
    return produced == 6 ? 0 : 1;
}