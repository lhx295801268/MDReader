#include "services/OutlineExtractor.h"
#include <QRegularExpression>

namespace {
QString slugify(const QString& s) {
    QString r = s.toLower();
    // Keep ASCII letters/digits, CJK ideographs (basic block), whitespace, hyphens.
    r.replace(QRegularExpression(R"([^a-z0-9一-鿿\s\-])"), QString());
    // Collapse whitespace runs to a single hyphen.
    r.replace(QRegularExpression(R"(\s+)", QRegularExpression::CaseInsensitiveOption), "-");
    return r;
}
}  // namespace

QList<OutlineExtractor::Entry>
OutlineExtractor::extract(const QString& md) {
    QList<Entry> out;
    QRegularExpression headRe(R"(^(#{1,6})\s+(.+?)\s*$)",
                              QRegularExpression::MultilineOption);

    bool inFence = false;
    int pos = 0;
    int curLine = 1;
    while (pos <= md.size()) {
        // Find end of current line (or end of string).
        int eol = md.indexOf('\n', pos);
        QString lineStr = (eol < 0) ? md.mid(pos) : md.mid(pos, eol - pos);

        // Detect triple-backtick fence toggles on this line.
        if (lineStr.contains(QStringLiteral("```"))) inFence = !inFence;

        // Only match headings outside fences.
        if (!inFence) {
            auto m = headRe.match(lineStr);
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
    return out;
}
