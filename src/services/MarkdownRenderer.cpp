#include "services/MarkdownRenderer.h"
#include "services/MermaidTheme.h"
#include "services/OutlineExtractor.h"
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
    // editor's link targets resolve to the right block. Slug rules live in
    // OutlineExtractor::slugify so outline entries and preview anchors stay
    // in lockstep — do NOT duplicate the regex here.
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
        const QString slug = OutlineExtractor::slugify(inner);
        processed += QString("<%1 id=\"%2\"%3>%4</%1>")
                         .arg(tag, slug, attrs, inner);
        last = m.capturedEnd();
    }
    if (processed.isEmpty()) processed = bodyHtml;
    else processed += bodyHtml.mid(last);

    const QString themeCss = loadQrcOrEmpty(QStringLiteral(":/themes/%1.css").arg(themeName));

    // Mermaid is a 3.5 MB bundle. Live-render mode reloads the whole page on
    // every debounced keystroke, so loading it unconditionally would make
    // every edit in a diagram-free document pay that parse cost for nothing.
    //
    // Case-insensitive: cmark passes the fence info string through verbatim,
    // so ```Mermaid yields class="language-Mermaid". A case-sensitive probe
    // would skip the injection and leave the diagram silently unrendered,
    // which is worse than the wasted parse this check exists to avoid.
    const bool hasMermaid = processed.contains(QLatin1String("language-mermaid"),
                                               Qt::CaseInsensitive);

    // Deliberate order:
    //   convert() -> MathJax -> hljs -> render()
    // convert() swaps the mermaid fences out of the DOM before highlight.js
    // can mis-highlight diagram source. Mermaid runs last AND is chained on
    // the MathJax promise rather than merely written after it: MathJax's
    // default inline math is $$...$$, so a diagram whose labels contain a
    // dollar-delimited span could otherwise be typeset in place while
    // mermaid is still reading that element's text as its source.
    static const char kBootstrapJs[] = R"JS(
window.addEventListener('DOMContentLoaded', function () {
  var diagrams = window.MDReaderMermaid;
  if (diagrams) diagrams.convert();
  var mathDone;
  if (window.MathJax && MathJax.typesetPromise) {
    mathDone = MathJax.typesetPromise().catch(function (e) { console.error(e); });
  } else {
    document.body.insertAdjacentHTML('beforeend',
      '<div style="position:fixed;top:0;left:0;right:0;background:#fdf6c4;color:#000;padding:6px;text-align:center;font:12px sans-serif;">MathJax unavailable, math not rendered</div>');
    mathDone = Promise.resolve();
  }
  if (window.hljs) hljs.highlightAll();
  if (diagrams) mathDone.then(function () { diagrams.render(); });
});
)JS";

    QString page;
    page += QStringLiteral("<!doctype html><html><head><meta charset='utf-8'>");
    page += QStringLiteral("<style>") + themeCss + QStringLiteral("</style>");
    page += QStringLiteral("<script src='qrc:/vendor/highlight/highlight.min.js'></script>");
    page += QStringLiteral("<script src='qrc:/vendor/mathjax/tex-mml-chtml.js'></script>");
    if (hasMermaid) {
        page += QStringLiteral("<script src='qrc:/vendor/mermaid/mermaid.min.js'></script>");
        page += QStringLiteral("<script src='qrc:/vendor/mermaid/mermaid-init.js'></script>");
    }
    page += QStringLiteral("<script>") + QLatin1String(kBootstrapJs) + QStringLiteral("</script>");
    page += QStringLiteral("</head><body data-mermaid-theme='")
          + mdreader::mermaid::mermaidThemeFor(themeName)
          + QStringLiteral("'>");
    page += processed;
    page += QStringLiteral("</body></html>");
    return page;
}