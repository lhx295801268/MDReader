#include "services/WordCounter.h"
#include <QRegularExpression>

WordCounter::Stats WordCounter::count(const QString& md) {
    Stats s;
    s.chars = md.size();

    // Words: contiguous runs of word chars (incl. CJK range 一-鿿).
    QRegularExpression wordsRe(R"([\w一-鿿]+)");
    s.words = md.count(wordsRe);

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
    // Tail check: trailing content after the last blank-line separator also
    // counts as a paragraph — the original loop omitted this.
    if (prevEnd < md.size() && !md.mid(prevEnd).trimmed().isEmpty()) {
        ++s.paragraphs;
    }
    // Fallback: no separators but content exists.
    if (s.paragraphs == 0 && !md.trimmed().isEmpty()) s.paragraphs = 1;

    // Headings: # ... ###### (must be followed by space + non-space).
    QRegularExpression headRe(R"(^#{1,6}\s+\S)", QRegularExpression::MultilineOption);
    s.headings = md.count(headRe);

    return s;
}
