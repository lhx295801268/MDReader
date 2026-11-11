#include <QtTest>
#include <QWindowStateChangeEvent>
#include "app/MainWindow.h"

// Integration test for the macOS fullscreen round-trip.
//
// The bug: leaving fullscreen on macOS makes the window come back
// *zoomed*, so Qt reports a state change of (old = FullScreen,
// now = Maximized). That looks exactly like "the window manager just
// maximized a window", which is the trigger MainWindow::changeEvent
// uses to redirect a title-bar double-click into fullscreen. The user
// double-clicked to leave fullscreen, the window shrank for a frame —
// and was yanked back into fullscreen.
//
// The sequence this test walks through is the one a real user produces:
// the window manager maximizes the window (title-bar double-click),
// MainWindow::changeEvent redirects that into fullscreen, and the user
// then asks to leave fullscreen again.
//
// Note the test drives the window state directly instead of synthesising
// mouse events: the double-click itself is owned by the window manager
// and never reaches the application.
class FullscreenToggleTest : public QObject {
    Q_OBJECT
private slots:
    void leaving_fullscreen_stays_out();
};

void FullscreenToggleTest::leaving_fullscreen_stays_out() {
    MainWindow w;
    w.resize(900, 600);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    // The window manager maximizes the window (title-bar double-click).
    w.setWindowState(w.windowState() | Qt::WindowMaximized);
    QTest::qWait(800);
    // MainWindow::changeEvent redirects that into fullscreen.
    QVERIFY(w.windowState() & Qt::WindowFullScreen);

    // Now leave fullscreen — the toolbar double-click / Esc / View menu
    // all do exactly this. macOS restores the zoomed frame, which shows
    // up as (old = FullScreen, now = Maximized) and must not be
    // redirected back into fullscreen.
    w.setWindowState(w.windowState() & ~Qt::WindowFullScreen);
    QTest::qWait(1200);

    const bool bouncedBack = w.windowState() & Qt::WindowFullScreen;
    const bool zoomRestored = w.windowState() & Qt::WindowMaximized;
    if (!bouncedBack && !zoomRestored) {
        // No window manager restored a zoomed frame (the offscreen QPA
        // platform used in CI has none), so the transition that used to
        // bounce the window back never happens here. The deterministic
        // guard for that is unit_FullscreenRedirectTest; this test is the
        // end-to-end proof on a real desktop.
        QSKIP("this platform does not restore a zoomed frame — the bounce cannot occur");
    }
    QVERIFY2(!bouncedBack, "the window bounced back into fullscreen");
}

QTEST_MAIN(FullscreenToggleTest)
#include "FullscreenToggleTest.moc"
