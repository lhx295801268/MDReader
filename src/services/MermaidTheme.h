#pragma once
// Maps a preview theme name (the same string MarkdownRenderer uses to pick
// themes/<name>.css) onto Mermaid's built-in theme. Header-only and pure so
// the mapping can be unit-tested without a WebEngine stack.
//
// The two-tier choice is deliberate: Mermaid ships a good `default` (light)
// and `dark` theme, and hand-tuning themeVariables for all six palettes would
// mean six sets of colour values to keep in sync with the CSS. What actually
// matters for readability is that a diagram never lands as a white slab in a
// dark document. When a seventh theme is added, it only has to be sorted into
// the light or dark list below.
#include <QString>

namespace mdreader::mermaid {

// Themes whose stylesheet sets a dark body background. Keep in lockstep with
// src/resources/themes/*.css — every entry here should be a stylesheet whose
// `body { background: ... }` is dark.
inline bool isDarkTheme(const QString& themeName) {
    return themeName == QLatin1String("github-dark")
        || themeName == QLatin1String("dracula")
        || themeName == QLatin1String("solarized-dark")
        || themeName == QLatin1String("one-dark");
}

// The value written to <body data-mermaid-theme="...">. mermaid-init.js reads
// this attribute and passes it straight to mermaid.initialize().
inline QString mermaidThemeFor(const QString& themeName) {
    return isDarkTheme(themeName) ? QStringLiteral("dark") : QStringLiteral("light");
}

}  // namespace mdreader::mermaid
