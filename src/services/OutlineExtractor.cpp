#include "services/OutlineExtractor.h"
#include <QRegularExpression>
#include <QDebug>

// File-scope regexes. QRegularExpression parses + compiles its pattern at
// construction, so we hoist these out of hot paths (slugify is called per
// heading; headRe is rebuilt on every extract() call which Phase 2/3 will
// invoke on debounced edits).
static const QRegularExpression kSlugStripRe(R"([^a-z0-9一-鿿\s\-])");
static const QRegularExpression kSlugCollapseWsRe(R"(\s+)");

static const QRegularExpression kHeadRe(
    R"(^(#{1,6})\s+(.+?)\s*$)",
    QRegularExpression::MultilineOption);

QString OutlineExtractor::slugify(const QString& s) {
    QString r = s.toLower();
    // Keep ASCII letters/digits, CJK ideographs (basic block), whitespace, hyphens.
    r.replace(kSlugStripRe, QString());
    // Collapse whitespace runs to a single hyphen.
    r.replace(kSlugCollapseWsRe, QStringLiteral("-"));
    return r;
}

QList<OutlineExtractor::Entry>
OutlineExtractor::extract(const QString& md) {
    QList<Entry> out;

    bool inFence = false;
    int pos = 0;
    int curLine = 1;
    while (pos < md.size()) {
        // Find end of current line (or end of string).
        int eol = md.indexOf('\n', pos);
        QString lineStr = (eol < 0) ? md.mid(pos) : md.mid(pos, eol - pos);

        // Detect triple-backtick fence toggles on this line.
        if (lineStr.contains(QStringLiteral("```"))) inFence = !inFence;

        // Only match headings outside fences.
        if (!inFence) {
            auto m = kHeadRe.match(lineStr);
            if (m.hasMatch()) {
                Entry e;
                e.level = m.captured(1).size();
                e.text = m.captured(2).trimmed();
                e.lineNumber = curLine;
                e.slug = slugify(e.text);
                out.push_back(e);
            }
        }

        if (eol < 0) break;
        pos = eol + 1;
        ++curLine;
    }

    // I1: diagnostic — if the document ends inside a fence, every subsequent
    // heading is silently dropped. Purely a warning; we still return what we
    // collected up to this point.
    if (inFence) {
        qWarning() << "OutlineExtractor: unclosed code fence at end of document;"
                   << "headings after the opening fence were ignored.";
    }

    return out;
}
