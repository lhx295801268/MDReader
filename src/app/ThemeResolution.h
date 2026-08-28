#pragma once
// Phase 7: pure resolver for "what CSS theme should the preview render
// with right now?" Extracted into a header-only module so unit tests can
// cover every branch without linking the entire MainWindow translation
// unit (which transitively pulls in QWebEngineWidgets, cmark-gfm, every
// Document* / Render* / FileWatcher, etc.).
//
// The resolver is a pure function of three inputs:
//   - mode            : "auto" (follow OS) or anything else (use userTheme)
//   - userTheme       : the concrete theme basename the user last picked,
//                       only consulted when mode != "auto"
//   - systemScheme    : OS colorScheme at the time of the call (own enum,
//                       translation layer lives at the call site)
//
// Only "auto" consults systemScheme; any other mode returns userTheme
// verbatim, even when the OS flips to dark — once the user has pinned a
// concrete theme their choice wins until they switch back to Follow System.
//
// We define our own ColorScheme enum (instead of Qt::ColorScheme) so the
// header compiles on Qt 6.2 — Qt::ColorScheme was added in Qt 6.5. The
// call site (MainWindow) translates Qt::ColorScheme -> SystemColorScheme
// when the platform API is available.
#include <QString>

namespace mdreader::theme {

// Three-state enum mirrors Qt::ColorScheme semantically; same numeric
// values as Qt::ColorScheme so callers can static_cast when the Qt enum
// is available.
enum class SystemColorScheme : int { Unknown = 0, Light = 1, Dark = 2 };

// Sentinel values shared between MainWindow (writes QSettings keys) and
// ThemeMenu (the "Follow System" menu entry emits "auto").
inline constexpr auto kModeAuto        = "auto";
inline constexpr auto kModeManual      = "manual";
// Phase 7: follow-system mode maps the OS colorScheme onto these two CSS
// basenames. They are the most battle-tested themes in the bundle and
// give the best experience in their respective brightness regimes.
inline constexpr auto kAutoLightTheme  = "github";
inline constexpr auto kAutoDarkTheme   = "github-dark";

inline QString resolveTheme(const QString& mode,
                            const QString& userTheme,
                            SystemColorScheme systemScheme) {
    if (mode == QLatin1String(kModeAuto)) {
        // Unknown (the transient startup value before Qt has probed the
        // platform) falls in the light branch — same default the rest of
        // the app uses for kAutoLightTheme. We'd rather render light
        // than crash on a bad enum comparison.
        return systemScheme == SystemColorScheme::Dark
            ? QLatin1String(kAutoDarkTheme)
            : QLatin1String(kAutoLightTheme);
    }
    return userTheme;
}

}  // namespace mdreader::theme