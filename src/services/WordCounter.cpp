#include "services/WordCounter.h"
#include <QRegularExpression>

WordCounter::Stats WordCounter::count(const QString& md) {
    Stats s;
    s.chars = md.size();

    // Words: contiguous runs of word chars (incl. CJK range 一-鿿).
    QRegularExpression wordsRe(R"([\w一-鿿]+)");
    auto it = wordsRe.globalMatch(md);
    while (it.hasNext()) { ++s.words; it.next(); }

    // Paragraphs: count non-empty content segments between blank-line
    // separators (^\s*$|\n\s*\n). Each segment with content before/after
    // a separator counts as one paragraph. Headings are included.
    QRegularExpression paraRe(R"(^\s*$|\n\s*\n)", QRegularExpression::MultilineOption);
    int prevEnd = 0;
    for (auto m = paraRe.globalMatch(md); m.hasNext(); ) {
        auto x = m.next();
        if (x.capturedStart() > prevEnd) ++s.paragraphs;  // content before this separator
        prevEnd = x.capturedEnd();
    }
    // FIX: also count content after the last separator (the plan's snippet
    // missed this, causing the english test to fail: "# Hello world\n\nThis *is* a test."
    // expects paragraphs == 2 because both the heading line and the trailing
    // sentence are content segments).
    if (prevEnd < md.size() && !md.mid(prevEnd).trimmed().isEmpty()) {
        ++s.paragraphs;
    }
    // Fallback: no separators but content exists.
    if (s.paragraphs == 0 && !md.trimmed().isEmpty()) s.paragraphs = 1;

    // Headings: # ... ###### (must be followed by space + non-space).
    QRegularExpression headRe(R"(^(#{1,6})\s+\S)", QRegularExpression::MultilineOption);
    auto h = headRe.globalMatch(md);
    while (h.hasNext()) { ++s.headings; h.next(); }

    return s;
}
