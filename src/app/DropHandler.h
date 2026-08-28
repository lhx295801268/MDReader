#pragma once
// Drag-and-drop helper: turn a QMimeData payload into the list of local
// Markdown file paths that should be opened. Lives in a header so unit
// tests can hit it without bringing up the full MainWindow / QtWebEngine
// stack — Drag-drop UX is small but easy to get wrong (URLs may be
// remote, may have non-file schemes, may include folders), so we cover
// every branch.
#include <QMimeData>
#include <QStringList>
#include <QUrl>
#include <QFileInfo>
#include <Qt>

namespace mdreader::drop {

// Returns true when the drop event payload looks like at least one local
// file we could plausibly open — used by dragEnterEvent / dragMoveEvent
// to decide whether to acceptProposedAction(). Cheap: only inspects the
// URL list, doesn't touch the filesystem.
inline bool canAccept(const QMimeData* mime) {
    if (!mime) return false;
    if (!mime->hasUrls()) return false;
    for (const QUrl& u : mime->urls()) {
        if (!u.isLocalFile()) continue;
        // Accept any local file — we filter by extension at open time so
        // dropping a folder of mixed files doesn't get silently dropped.
        return true;
    }
    return false;
}

// Lower-cased suffixes we treat as Markdown. .md is the canonical one;
// the rest are common spellings used by other editors. Comparison is
// case-insensitive (case-insensitive suffixes are mandated by RFC).
inline bool isMarkdownSuffix(const QString& suffix) {
    const QString s = suffix.toLower();
    return s == QLatin1String("md")
        || s == QLatin1String("markdown")
        || s == QLatin1String("mdown")
        || s == QLatin1String("mkd")
        || s == QLatin1String("mkdn")
        || s == QLatin1String("mdtxt");
}

// Returns absolute paths of local Markdown files in `mime`, preserving
// the order URLs appear in the payload. Drops:
//   - remote URLs (http://, file:// on a non-mounted share)
//   - non-files (directories, sockets)
//   - files whose suffix is not in isMarkdownSuffix()
//
// The suffix filter matters because a user dragging a folder onto the
// window would otherwise get every file inside it as a tab. We only
// open .md-ish files.
inline QStringList extractLocalMarkdownPaths(const QMimeData* mime) {
    QStringList out;
    if (!mime || !mime->hasUrls()) return out;
    for (const QUrl& u : mime->urls()) {
        if (!u.isValid() || !u.isLocalFile()) continue;
        const QString p = u.toLocalFile();
        if (p.isEmpty()) continue;
        QFileInfo fi(p);
        if (!fi.exists() || !fi.isFile()) continue;     // skip folders, dangling links
        if (!isMarkdownSuffix(fi.suffix())) continue;
        out << fi.absoluteFilePath();
    }
    return out;
}

}  // namespace mdreader::drop