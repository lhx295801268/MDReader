#pragma once
#include <QString>
#include <QList>

class OutlineExtractor {
public:
    struct Entry { int level; QString text; int lineNumber; QString slug; };
    [[nodiscard]] static QList<Entry> extract(const QString& md);

    /// Slugify a heading string for use as an HTML id anchor.
    /// Mirrors the regex used by MarkdownRenderer::wrapHtml so outline entries
    /// and preview <hN id="..."> anchors stay in lockstep. If this ever
    /// changes, update MarkdownRenderer's heading-id injection in the same
    /// commit.
    [[nodiscard]] static QString slugify(const QString& text);
};