#pragma once
// "Export HTML" writes the live preview page to disk. That page loads
// highlight.js / MathJax / Mermaid through <script src="qrc:/vendor/...">,
// and a qrc: URL only resolves inside the application — an exported file
// opened in a browser silently loses code highlighting, math and diagrams.
// Rewriting each qrc: reference into an inline <script> makes the export
// standalone. The cost is size: roughly 5 MB once every vendor bundle is
// embedded.
//
// Header-only and free of Qt widgets so it is testable without spinning up a
// WebEngine stack.
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QtGlobal>

namespace mdreader::html {

// Upper bound on how much vendored JS we will inline. A legitimate standalone
// export is ~5 MB; a much larger number means the page referenced something we
// did not expect, and silently writing a 100 MB "export" would be worse than
// leaving that one tag alone.
inline constexpr qint64 kMaxInlinedBytes = 16 * 1024 * 1024;

// Reads a qrc: path through QFile. Accepts both spellings Qt allows
// ("qrc:/vendor/x.js" and ":/vendor/x.js") and normalises to the ":/" form.
inline QString loadQrc(const QString& qrcPath) {
    const QString path = qrcPath.startsWith(QLatin1String("qrc:"))
                             ? QStringLiteral(":") + qrcPath.mid(4)
                             : qrcPath;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("mdreader::html::loadQrc: cannot open %s", qUtf8Printable(path));
        return {};
    }
    if (f.size() > kMaxInlinedBytes) {
        qWarning("mdreader::html::loadQrc: %s is %lld bytes, above the %lld byte inline cap",
                 qUtf8Printable(path), static_cast<long long>(f.size()),
                 static_cast<long long>(kMaxInlinedBytes));
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

// Replaces every <script src="qrc:...">...</script> with the resource's
// contents wrapped in a plain <script>. Tags whose resource cannot be read
// (or is over the cap) are left exactly as they were, so a failed inline
// degrades to today's behaviour instead of dropping the tag.
inline QString inlineQrcScripts(const QString& html) {
    static const QRegularExpression kQrcScriptRe(
        QStringLiteral(R"(<script\s+src=(['"])(qrc:[^'"]+)\1\s*>\s*</script>)"));

    QString out;
    out.reserve(html.size());
    int last = 0;
    auto it = kQrcScriptRe.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += html.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();

        QString js = loadQrc(m.captured(2));
        if (js.isEmpty()) {
            out += m.captured(0);   // leave the tag untouched
            continue;
        }
        // Inside a <script> element the HTML parser looks for the literal
        // "</script" anywhere, including inside JS strings and comments. The
        // escape is a no-op for JS semantics ("\/" === "/" in a string or
        // regex, plain text in a comment) and stops the script from being
        // truncated. No current vendor bundle contains the sequence — this
        // guards against a future one that does.
        js.replace(QLatin1String("</script"), QLatin1String("<\\/script"));
        out += QStringLiteral("<script>") + js + QStringLiteral("</script>");
    }
    out += html.mid(last);
    return out;
}

}  // namespace mdreader::html
