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
//   - systemScheme    : QStyleHints::colorScheme() at the time of the call
//
// Only "auto" consults systemScheme; any other mode returns userTheme
// verbatim, even when the OS flips to dark — once the user has pinned a
// concrete theme their choice wins until they switch back to Follow System.
#include <QString>
#include <Qt>

namespace mdreader::theme {

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
                            Qt::ColorScheme systemScheme) {
    if (mode == QLatin1String(kModeAuto)) {
        // Unknown (the transient startup value before Qt has probed the
        // platform) falls in the light branch — same default the rest of
        // the app uses for kAutoLightTheme. We'd rather render light
        // than crash on a bad enum comparison.
        return systemScheme == Qt::ColorScheme::Dark
            ? QLatin1String(kAutoDarkTheme)
            : QLatin1String(kAutoLightTheme);
    }
    return userTheme;
}

}  // namespace mdreader::theme