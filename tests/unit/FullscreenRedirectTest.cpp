#include <QtTest>
#include "app/FullscreenRedirect.h"

// Unit test for the fullscreen redirect state machine
// (app/FullscreenRedirect.h).
//
// The event sequences below are the ones macOS actually produces; they
// were captured from a real MainWindow while watching WindowStateChange:
//
//   entering fullscreen (title-bar double-click):
//       old=NoState   now=Maximized      → redirect to fullscreen
//       old=Maximized now=FullScreen     → settled
//
//   leaving fullscreen:
//       old=FullScreen now=NoState        → out of fullscreen
//       old=NoState   now=Maximized      → the WM restores the zoomed
//                                          frame — this looks exactly
//                                          like the double-click, and
//                                          redirecting it snaps the user
//                                          straight back into fullscreen
//       old=Maximized now=NoState        → settled
//
// The redirect is a pure decision, so no window server is needed — the
// same pattern as TitleBarFullscreenTest.
class FullscreenRedirectTest : public QObject {
    Q_OBJECT
private slots:
    void maximize_from_normal_becomes_fullscreen();
    void zoom_restore_after_leaving_fullscreen_is_not_redirected();
    void leaving_fullscreen_directly_to_maximized_is_not_redirected();
    void maximize_after_the_zoom_restore_redirects_again();
    void awaiting_zoom_restore_is_reported();
    void timeout_clears_the_pending_zoom_restore();
};

using mdreader::window::FullscreenRedirect;

void FullscreenRedirectTest::maximize_from_normal_becomes_fullscreen() {
    FullscreenRedirect r;
    const Qt::WindowStates target = r.apply(Qt::WindowMaximized, Qt::WindowNoState);
    QVERIFY(target & Qt::WindowFullScreen);
    QVERIFY(!(target & Qt::WindowMaximized));
}

void FullscreenRedirectTest::zoom_restore_after_leaving_fullscreen_is_not_redirected() {
    FullscreenRedirect r;
    // The window leaves fullscreen…
    QCOMPARE(r.apply(Qt::WindowNoState, Qt::WindowFullScreen), Qt::WindowNoState);
    QVERIFY(r.awaitingZoomRestore());
    // …and macOS restores the zoomed frame. This is the transition that
    // used to bounce the user back into fullscreen.
    const Qt::WindowStates target = r.apply(Qt::WindowMaximized, Qt::WindowNoState);
    QCOMPARE(target, Qt::WindowMaximized);
    QVERIFY(!(target & Qt::WindowFullScreen));
    // The zoom restore was consumed — a later maximize is a genuine
    // request again.
    QVERIFY(!r.awaitingZoomRestore());
}

void FullscreenRedirectTest::leaving_fullscreen_directly_to_maximized_is_not_redirected() {
    // Some macOS versions report the exit as a single
    // (old=FullScreen, now=Maximized) transition.
    FullscreenRedirect r;
    const Qt::WindowStates target = r.apply(Qt::WindowMaximized, Qt::WindowFullScreen);
    QCOMPARE(target, Qt::WindowMaximized);
    QVERIFY(!(target & Qt::WindowFullScreen));
}

void FullscreenRedirectTest::maximize_after_the_zoom_restore_redirects_again() {
    FullscreenRedirect r;
    r.apply(Qt::WindowNoState, Qt::WindowFullScreen);   // leaving fullscreen
    r.apply(Qt::WindowMaximized, Qt::WindowNoState);     // the zoom restore
    // Now the user double-clicks the title bar again — that must go
    // fullscreen, the way it did before.
    const Qt::WindowStates target = r.apply(Qt::WindowMaximized, Qt::WindowNoState);
    QVERIFY(target & Qt::WindowFullScreen);
}

void FullscreenRedirectTest::awaiting_zoom_restore_is_reported() {
    FullscreenRedirect r;
    QVERIFY(!r.awaitingZoomRestore());
    r.apply(Qt::WindowNoState, Qt::WindowFullScreen);
    QVERIFY(r.awaitingZoomRestore());
}

void FullscreenRedirectTest::timeout_clears_the_pending_zoom_restore() {
    // Not every platform sends a zoom restore. When it doesn't, the
    // pending state has to expire — otherwise a later legitimate
    // maximize would be swallowed.
    FullscreenRedirect r;
    r.apply(Qt::WindowNoState, Qt::WindowFullScreen);
    QVERIFY(r.awaitingZoomRestore());
    r.zoomRestoreTimedOut();
    QVERIFY(!r.awaitingZoomRestore());
    const Qt::WindowStates target = r.apply(Qt::WindowMaximized, Qt::WindowNoState);
    QVERIFY(target & Qt::WindowFullScreen);
}

QTEST_APPLESS_MAIN(FullscreenRedirectTest)
#include "FullscreenRedirectTest.moc"
