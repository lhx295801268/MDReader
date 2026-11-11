#pragma once
#include <Qt>

#include "app/TitleBarFullscreen.h"

// The state machine behind "title-bar double-click → fullscreen".
//
// The native title bar belongs to the window manager, so the application
// never sees the double-click — only the WM's reaction: the window gets
// maximized, and Qt reports that as a WindowStateChange. The pure helper
// in app/TitleBarFullscreen.h turns that into fullscreen.
//
// macOS adds a wrinkle: when the window leaves fullscreen, the WM
// restores the window's *zoomed* frame, and Qt reports that as a plain
// maximize transition — old state already NoState, so it is
// indistinguishable from the title-bar double-click we redirect into
// fullscreen. Redirecting it would shrink the window for a frame and snap
// it straight back.
//
// The helper below therefore remembers that the window is on its way out
// of fullscreen and lets exactly one maximize transition through
// untouched. The rule itself is pure, but the "did we just leave
// fullscreen" bit is state that has to survive across events — hence a
// small class rather than another free function.
//
// Header-only (no Q_OBJECT) so the transitions can be unit-tested
// without a window server — same pattern as app/TitleBarFullscreen.h.
namespace mdreader::window {

class FullscreenRedirect {
public:
    // The window state to apply for the given WM transition.
    Qt::WindowStates apply(Qt::WindowStates now, Qt::WindowStates old) {
        if (leavingFullScreen(now, old)) {
            awaitingZoomRestore_ = true;
        }
        if (awaitingZoomRestore_) {
            if (now & Qt::WindowMaximized) {
                // That was the zoom restore macOS performs on the way out
                // of fullscreen. Consumed — a later maximize transition is
                // a genuine request again.
                awaitingZoomRestore_ = false;
            }
            return now;  // never redirect while on the way out
        }
        return redirectTitleBarMaximize(now, old);
    }

    // True while the window is on its way out of fullscreen and the zoom
    // restore is still expected.
    bool awaitingZoomRestore() const { return awaitingZoomRestore_; }

    // The zoom restore never arrived (not every platform sends one) —
    // stop swallowing maximize transitions.
    void zoomRestoreTimedOut() { awaitingZoomRestore_ = false; }

private:
    static bool leavingFullScreen(Qt::WindowStates now, Qt::WindowStates old) {
        return (old & Qt::WindowFullScreen) && !(now & Qt::WindowFullScreen);
    }

    bool awaitingZoomRestore_ = false;
};

}  // namespace mdreader::window
