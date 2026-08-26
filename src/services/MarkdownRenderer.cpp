#include "services/MarkdownRenderer.h"
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include <cstdlib>
#include <mutex>
#include <QFile>
#include <QRegularExpression>

namespace {
QString loadQrcOrEmpty(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

// Ensure cmark-gfm core extensions (table, strikethrough, autolink, tasklist,
// tagfilter) are registered with the cmark plugin registry exactly once per
// process. Subsequent calls are no-ops. Phase 3 may attach additional
// extensions (strikethrough, autolinks, task lists) to the parser.
void ensureCoreExtensionsRegistered() {
    static std::once_flag flag;
    std::call_once(flag, cmark_gfm_core_extensions_ensure_registered);
}
}  // namespace

MarkdownRenderer::MarkdownRenderer() {
    ensureCoreExtensionsRegistered();
}

MarkdownRenderer::~MarkdownRenderer() = default;

QString MarkdownRenderer::render(const QString& markdown, const QString& themeName) {
    std::lock_guard<std::mutex> lk(mtx_);
    const QByteArray utf8 = markdown.toUtf8();
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE);

    // Enable GFM table extension. The table extension is registered by
    // cmark_gfm_core_extensions_ensure_registered() (called once in the
    // ctor). Without this attach, pipe-table markdown renders as a paragraph.
    static cmark_syntax_extension* table_ext = cmark_find_syntax_extension("table");
    if (table_ext) {
        cmark_parser_attach_syntax_extension(parser, table_ext);
    } else {
        static std::once_flag warn_flag;
        std::call_once(warn_flag, []() {
            qWarning("MarkdownRenderer: cmark-gfm 'table' extension is not registered; "
                     "pipe tables will render as plain text. Check that the cmark-gfm "
                     "core extensions library is linked and registered.");
        });
    }

    cmark_parser_feed(parser, utf8.constData(), utf8.size());
    cmark_node* doc = cmark_parser_finish(parser);
    if (!doc) {
        cmark_parser_free(parser);
        return wrapHtml(QStringLiteral("<p class=\"parse-error\">(parse error)</p>"), themeName);
    }
    char* html_c = cmark_render_html(doc, CMARK_OPT_DEFAULT, nullptr);
    cmark_parser_free(parser);
    cmark_node_free(doc);
    if (!html_c) {
        return wrapHtml(QStringLiteral("<p class=\"render-error\">(render failure)</p>"), themeName);
    }
    QString body = QString::fromUtf8(html_c);
    std::free(html_c);
    QString wrapped = wrapHtml(body, themeName);
    return wrapped;
}

QString MarkdownRenderer::wrapHtml(const QString& bodyHtml, const QString& themeName) {
    // Task 20: inject id="<slug>" on h1..h6 so the preview's JS scroll
    // (OutlineView::headingActivated → page().runJavaScript) and the
    // editor's link targets resolve to the right block. Slug rules must
    // match OutlineExtractor::slugify so that outline entries map 1:1 to
    // preview anchors.
    static const QRegularExpression kSlugStripRe(R"([^a-z0-9一-鿿\s\-])");
    static const QRegularExpression kSlugCollapseWsRe(R"(\s+)");
    auto fixSlug = [](QString s) {
        s = s.toLower();
        s.replace(kSlugStripRe, QString());
        s.replace(kSlugCollapseWsRe, QStringLiteral("-"));
        return s;
    };
    // Match opening tag (with possibly existing attrs) + inner text + close.
    // Greedy single-line match (cmark emits each heading on its own line).
    static const QRegularExpression kHeadingRe(
        R"(<(h[1-6])([^>]*)>([^<]+)</\1>)");
    QString processed;
    int last = 0;
    auto it = kHeadingRe.globalMatch(bodyHtml);
    while (it.hasNext()) {
        auto m = it.next();
        processed += bodyHtml.mid(last, m.capturedStart() - last);
        const QString tag = m.captured(1);
        const QString attrs = m.captured(2);
        const QString inner = m.captured(3);
        const QString slug = fixSlug(inner);
        processed += QString("<%1 id=\"%2\"%3>%4</%1>")
                         .arg(tag, slug, attrs, inner);
        last = m.capturedEnd();
    }
    if (processed.isEmpty()) processed = bodyHtml;
    else processed += bodyHtml.mid(last);

    const QString themeCss = loadQrcOrEmpty(QStringLiteral(":/themes/%1.css").arg(themeName));
    return QStringLiteral(
        "<!doctype html><html><head>"
        "<meta charset='utf-8'>"
        "<style>%1</style>"
        "<script src='qrc:/vendor/highlight/highlight.min.js'></script>"
        "<script src='qrc:/vendor/mathjax/tex-mml-chtml.js'></script>"
        "<script>"
        "window.addEventListener('DOMContentLoaded', function() {"
        "  if (window.MathJax) MathJax.typesetPromise();"
        "  if (window.hljs) hljs.highlightAll();"
        "});"
        "</script>"
        "</head><body>%2</body></html>"
    ).arg(themeCss, processed);
}