#pragma once
#include <Qt>

// Phase 13: title-bar double-click → fullscreen.
//
// The native title bar is owned by the window manager (WM), which means Qt
// never sees the double-click event itself. What Qt *does* see is the WM's
// reaction: WM maximizes the window → Qt fires a QEvent::WindowStateChange.
// We hook that event in changeEvent() and ask this helper what state we
// actually want, then call setWindowState() with the result.
//
// The reason we redirect maximize → fullscreen (and not just allow normal
// maximize) is that the user asked for "title bar double-click to enlarge
// the window". Maximize leaves the panel/taskbar visible; fullscreen hides
// everything. For a document reader that wants as much space as possible
// for the preview, fullscreen is the right semantics.
//
// Pure function: takes the new state Qt observed (`now`) and the previous
// state carried by QWindowStateChangeEvent::oldState() (`old`), and returns
// the state MainWindow should apply. No side effects.
namespace mdreader::window {

inline Qt::WindowStates redirectTitleBarMaximize(Qt::WindowStates now,
                                                  Qt::WindowStates old) {
    // Trigger condition: WM just transitioned into WindowMaximized from a
    // non-maximized state. Covers "normal → maximize" (the typical
    // title-bar dbl-click path on X11/Wayland) and "minimized → maximize"
    // (less common but still valid). The WM-driven unmaximize path
    // (Maximized → NoState) does NOT trigger — old & Maximized is true,
    // so the condition is false.
    const bool enteringMaximize = (now & Qt::WindowMaximized)
                               && !(old & Qt::WindowMaximized);
    if (enteringMaximize) {
        // Clear the Maximized flag the WM just set, replace with FullScreen.
        // Preserves any other flags (e.g. WindowActive) so focus isn't
        // disturbed by the redirect.
        return (now & ~Qt::WindowMaximized) | Qt::WindowFullScreen;
    }
    // No redirect applies. Returning `now` keeps the helper idempotent —
    // calling it twice with the same args returns the same value, which
    // matters because setWindowState() re-fires WindowStateChange and
    // MainWindow::changeEvent() will run us a second time on the
    // redirected state. On the second call `old` will already include
    // the flags from the redirect, so enteringMaximize is false, so we
    // return `now` unchanged. No infinite loop.
    return now;
}

} // namespace mdreader::window