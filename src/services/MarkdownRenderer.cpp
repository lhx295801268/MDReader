#include "services/MarkdownRenderer.h"
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include <cstdlib>
#include <mutex>
#include <QFile>

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
    }

    cmark_parser_feed(parser, utf8.constData(), utf8.size());
    cmark_node* doc = cmark_parser_finish(parser);
    if (!doc) {
        cmark_parser_free(parser);
        return QStringLiteral("<p>(parse error)</p>");
    }
    char* html_c = cmark_render_html(doc, CMARK_OPT_DEFAULT, nullptr);
    cmark_parser_free(parser);
    cmark_node_free(doc);
    QString body = QString::fromUtf8(html_c);
    std::free(html_c);
    QString wrapped = wrapHtml(body, themeName);
    return wrapped;
}

QString MarkdownRenderer::wrapHtml(const QString& bodyHtml, const QString& themeName) {
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
    ).arg(themeCss, bodyHtml);
}