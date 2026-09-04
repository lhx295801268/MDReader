#pragma once
// Phase 12: per-theme color palette for the left-hand editor pane.
//
// Each preview theme ships its own CSS for the right-hand WebEngine
// preview; until Phase 12 the editor kept a fixed white-on-black style
// regardless of which preview theme was active, which produced a jarring
// split look in dark themes. EditorPalette is the missing mirror of
// ThemeMenu — given a theme basename (the same string the preview uses),
// it returns the colors the editor should adopt.
//
// Keep the table small and explicit: a hand-tuned entry per theme beats a
// generic lightness heuristic because each theme's contrast requirements
// differ (Dracula's purple is fine on its own ground but harsh on white;
// solarized-light's tan needs a slightly darker line-number band to
// remain legible). When a new theme is added, add its row here AND drop
// its CSS in src/resources/themes/.
//
// isDark() drives the few code paths that need a boolean rather than
// comparing colors (e.g. picking a cursor caret color that always pops).
#include <QColor>
#include <QString>

namespace mdreader::theme {

struct EditorPalette {
    QColor background;     // viewport fill
    QColor foreground;     // body text
    QColor lineNumberBg;   // gutter strip on the left
    QColor lineNumberFg;   // line-number digits
    QColor selectionBg;    // selected-text highlight
    QColor selectionFg;    // text on top of the selection
    QColor caret;          // text cursor
    bool   isDark = false; // convenience for callers that want a bool
};

// Resolve a preview-theme basename to the editor palette used for it.
// Unknown names fall back to github (light) — same default the preview
// uses — so an unset / typo'd theme name at least produces a sensible
// look rather than a default-constructed QColor (which is invalid).
inline EditorPalette paletteForTheme(const QString& theme) {
    if (theme == QLatin1String("github")) {
        return {
            QColor("#ffffff"), QColor("#24292e"),
            QColor("#f6f8fa"), QColor("#959da5"),
            QColor("#cce5ff"), QColor("#24292e"),
            QColor("#0366d6"), false
        };
    }
    if (theme == QLatin1String("github-dark")) {
        return {
            QColor("#0d1117"), QColor("#c9d1d9"),
            QColor("#161b22"), QColor("#6e7681"),
            QColor("#3a3f4b"), QColor("#c9d1d9"),
            QColor("#58a6ff"), true
        };
    }
    if (theme == QLatin1String("dracula")) {
        return {
            QColor("#282a36"), QColor("#f8f8f2"),
            QColor("#44475a"), QColor("#6272a4"),
            QColor("#44475a"), QColor("#f8f8f2"),
            QColor("#bd93f9"), true
        };
    }
    if (theme == QLatin1String("solarized-light")) {
        return {
            QColor("#fdf6e3"), QColor("#586e75"),
            QColor("#eee8d5"), QColor("#93a1a1"),
            QColor("#eee8d5"), QColor("#586e75"),
            QColor("#268bd2"), false
        };
    }
    if (theme == QLatin1String("solarized-dark")) {
        return {
            QColor("#002b36"), QColor("#93a1a1"),
            QColor("#073642"), QColor("#586e75"),
            QColor("#073642"), QColor("#93a1a1"),
            QColor("#268bd2"), true
        };
    }
    if (theme == QLatin1String("one-dark")) {
        return {
            QColor("#282c34"), QColor("#abb2bf"),
            QColor("#3a3f4b"), QColor("#5c6370"),
            QColor("#3a3f4b"), QColor("#abb2bf"),
            QColor("#528bff"), true
        };
    }
    // Unknown / typo'd theme: keep the editor safe (light default).
    return paletteForTheme(QStringLiteral("github"));
}

}  // namespace mdreader::theme