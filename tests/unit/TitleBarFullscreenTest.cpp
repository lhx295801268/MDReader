#include <QtTest>
#include "app/TitleBarFullscreen.h"

// Phase 13: redirectTitleBarMaximize is a pure (now, old) → WindowStates
// function. Same testing pattern as MainWindowThemeTest (which avoids
// linking MainWindow.cpp by testing only its header-only helper): we
// exercise every documented edge case directly without spinning up a
// QApplication or MainWindow.
class TitleBarFullscreenTest : public QObject {
    Q_OBJECT
private slots:
    // The happy path: WM maximizes a normal window in response to a
    // title-bar double-click. We expect redirect to fullscreen.
    void normal_to_maximize_becomes_fullscreen();

    // Less common but still a valid trigger: user maximizes a minimized
    // window (e.g. clicks the taskbar entry which expands+maximizes on
    // some WMs). Same redirect applies.
    void minimized_to_maximize_becomes_fullscreen();

    // If we're already maximized and the WM re-fires the same state
    // (some WMs do this), no transition occurred — helper returns now
    // unchanged. Guards against accidental double-redirects.
    void already_maximized_no_change();

    // fullscreen → fullscreen (e.g. re-fire from setWindowState
    // recursion). Must be a no-op; otherwise we'd loop.
    void fullscreen_to_fullscreen_no_change();

    // The reverse path (fullscreen → normal via Esc or unmaximize key).
    // Helper should not try to redirect — we only intercept maximize, never
    // unmaximize.
    void fullscreen_to_normal_no_change();

    // Calling the helper twice with the same (now, old) must return the
    // same value both times. Property: redirect(now, old) is a pure
    // function — no internal state.
    void idempotent_repeated_calls();
};

using mdreader::window::redirectTitleBarMaximize;

void TitleBarFullscreenTest::normal_to_maximize_becomes_fullscreen() {
    const Qt::WindowStates old = Qt::WindowNoState;
    const Qt::WindowStates now = Qt::WindowMaximized;
    const Qt::WindowStates got = redirectTitleBarMaximize(now, old);
    // Expect: FullScreen set, Maximized cleared, other flags preserved.
    // (No "other" flags in this case, but we assert both bit operations
    // explicitly so a future regression on flag handling is caught.)
    QVERIFY(got & Qt::WindowFullScreen);
    QVERIFY(!(got & Qt::WindowMaximized));
}

void TitleBarFullscreenTest::minimized_to_maximize_becomes_fullscreen() {
    // Old state had WindowMinimized but not WindowMaximized — enteringMaximize
    // condition is "new has Maximized AND old doesn't", which is true here.
    const Qt::WindowStates old = Qt::WindowMinimized;
    const Qt::WindowStates now = Qt::WindowMaximized;
    const Qt::WindowStates got = redirectTitleBarMaximize(now, old);
    QVERIFY(got & Qt::WindowFullScreen);
    QVERIFY(!(got & Qt::WindowMaximized));
}

void TitleBarFullscreenTest::already_maximized_no_change() {
    // WM re-fires the same maximized state (some compositors do this on
    // focus regain). old had Maximized, so enteringMaximize is false.
    const Qt::WindowStates old = Qt::WindowMaximized;
    const Qt::WindowStates now = Qt::WindowMaximized;
    QCOMPARE(redirectTitleBarMaximize(now, old), now);
}

void TitleBarFullscreenTest::fullscreen_to_fullscreen_no_change() {
    // setWindowState() in MainWindow::changeEvent re-fires
    // WindowStateChange. On that second event the helper receives
    // (FullScreen, FullScreen|previous). Critically, neither side has
    // Maximized, so enteringMaximize is false → returns now unchanged.
    const Qt::WindowStates old = Qt::WindowFullScreen | Qt::WindowActive;
    const Qt::WindowStates now = Qt::WindowFullScreen | Qt::WindowActive;
    QCOMPARE(redirectTitleBarMaximize(now, old), now);
}

void TitleBarFullscreenTest::fullscreen_to_normal_no_change() {
    // The user pressed Esc (or otherwise exited fullscreen). Helper must
    // not "redirect" this transition — Esc → normal is the desired
    // outcome, not something we want to undo.
    const Qt::WindowStates old = Qt::WindowFullScreen;
    const Qt::WindowStates now = Qt::WindowNoState;
    QCOMPARE(redirectTitleBarMaximize(now, old), now);
}

void TitleBarFullscreenTest::idempotent_repeated_calls() {
    // Property: f(f(now, old)) == f(now, old). Run on a happy input twice.
    const Qt::WindowStates old = Qt::WindowNoState;
    const Qt::WindowStates now = Qt::WindowMaximized;
    const Qt::WindowStates first = redirectTitleBarMaximize(now, old);
    // Second call uses the first's output as `now`, and `now` (the input
    // Maximized) as `old`. That's the actual recursive shape
    // setWindowState() produces — but we ALSO want the trivial property
    // f(f(x)) == f(x) to hold, so test both shapes.
    const Qt::WindowStates trivialDouble =
        redirectTitleBarMaximize(first, first);
    QCOMPARE(trivialDouble, first);
    const Qt::WindowStates recursiveShape =
        redirectTitleBarMaximize(first, now);
    QCOMPARE(recursiveShape, first);
}

QTEST_APPLESS_MAIN(TitleBarFullscreenTest)
#include "TitleBarFullscreenTest.moc"