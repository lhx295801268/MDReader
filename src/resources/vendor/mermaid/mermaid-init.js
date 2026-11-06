/* mermaid-init.js — preview-side glue for Mermaid diagrams.
 *
 * Why this lives in the page instead of C++: the only thing that can turn
 * Mermaid source into an SVG is Mermaid itself, and it needs a real DOM
 * (text measurement, getBBox). Keeping the transform here leaves
 * MarkdownRenderer a pure markdown→HTML function that stays unit-testable.
 *
 * Contract with the host page (see MarkdownRenderer::wrapHtml):
 *   - `mermaid.min.js` is loaded before this file, but ONLY when the document
 *     actually contains a mermaid block.
 *   - `<body data-mermaid-theme="light|dark">` picks Mermaid's built-in theme.
 *   - The host calls convert() -> [MathJax] -> [hljs] -> render(), in that
 *     order. convert() must run before hljs so highlight.js never sees the
 *     diagram source, and render() runs last so MathJax never walks into a
 *     freshly-inserted SVG.
 *   - The zoom/pan overlay lives in viewer-overlay.js (window.MDReaderViewer),
 *     which is injected on EVERY page — a document may have no diagram at all
 *     and still have a large screenshot that needs zooming. This file only
 *     hands the rendered SVG to that shared viewer.
 *
 * Everything is idempotent: calling convert()/render() twice is a no-op.
 */
(function () {
  'use strict';

  // ---------------------------------------------------------------- styles

  // Injected rather than added to the six theme CSS files: the rules are
  // Mermaid-specific and theme-independent except for the error block, which
  // keys off body[data-mermaid-theme]. The zoom/pan overlay's own styles live
  // in viewer-overlay.js so they are present on diagram-free pages too.
  var CSS = [
    '.mdr-mermaid { margin: 20px 0; text-align: center; }',
    '.mdr-mermaid.is-rendered { cursor: zoom-in; }',
    '.mdr-mermaid-error { text-align: left; border: 1px solid #f0c0c0;',
    '  background: #fff5f5; border-radius: 6px; padding: 12px 14px;',
    '  font: 13px/1.5 SFMono-Regular, Menlo, Consolas, monospace; }',
    '.mdr-mermaid-error-head { font-weight: 600; color: #a03030; margin-bottom: 8px; }',
    '.mdr-mermaid-error-msg { margin: 0 0 8px; white-space: pre-wrap;',
    '  word-break: break-word; color: #a03030; }',
    '.mdr-mermaid-error details > summary { cursor: pointer; color: #6a737d; }',
    '.mdr-mermaid-error-src { margin: 8px 0 0; padding: 8px; overflow: auto;',
    '  background: rgba(27,31,35,.05); border-radius: 4px; white-space: pre; }',
    'body[data-mermaid-theme="dark"] .mdr-mermaid-error {',
    '  border-color: #7a4a4a; background: #2d1f1f; }',
    'body[data-mermaid-theme="dark"] .mdr-mermaid-error-head,',
    'body[data-mermaid-theme="dark"] .mdr-mermaid-error-msg { color: #ff9c9c; }',
    'body[data-mermaid-theme="dark"] .mdr-mermaid-error-src {',
    '  background: rgba(110,118,129,.25); }',
  ].join('\n');

  function injectStyle() {
    if (document.getElementById('mdr-mermaid-style')) return;
    var style = document.createElement('style');
    style.id = 'mdr-mermaid-style';
    style.textContent = CSS;
    (document.head || document.documentElement).appendChild(style);
  }

  // ------------------------------------------------------------- converting

  // cmark passes a fence's info string through verbatim, so ```Mermaid ends up
  // as class="language-Mermaid" and ```MERMAID as "language-MERMAID". Matching
  // the class case-sensitively would silently leave those blocks as plain code
  // with no diagram and no error — so normalise here rather than trusting the
  // document to spell it our way.
  function isMermaidCode(code) {
    var match = /\blanguage-([A-Za-z0-9_-]+)/.exec(code.getAttribute('class') || '');
    return !!match && match[1].toLowerCase() === 'mermaid';
  }

  // Swap every mermaid fence for a plain <div> carrying the same text.
  // cmark-gfm emits them as <pre><code class="language-mermaid">; after this
  // runs there is no <code> left for highlight.js to mis-highlight.
  function convert() {
    injectStyle();
    var codes = document.querySelectorAll('pre > code');
    var converted = 0;
    for (var i = 0; i < codes.length; i++) {
      var code = codes[i];
      if (!isMermaidCode(code)) continue;
      var pre = code.parentElement;
      if (!pre || !pre.parentNode) continue;
      var host = document.createElement('div');
      host.className = 'mdr-mermaid';
      host.textContent = code.textContent;
      pre.parentNode.replaceChild(host, pre);
      converted++;
    }
    return converted;
  }

  // -------------------------------------------------------------- rendering

  function currentTheme() {
    var attr = document.body && document.body.getAttribute('data-mermaid-theme');
    return attr === 'dark' ? 'dark' : 'default';
  }

  function initialize() {
    window.mermaid.initialize({
      startOnLoad: false,
      securityLevel: 'strict',
      theme: currentTheme(),
      fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
      flowchart: { htmlLabels: true, useMaxWidth: true },
      sequence: { useMaxWidth: true },
      gantt: { useMaxWidth: true },
    });
  }

  // Mermaid parks a scratch element with id "d" + id in the DOM while
  // rendering and does not always clean it up when the diagram fails to parse.
  //
  // Only the "d"-prefixed scratch is swept. The SVG mermaid hands back carries
  // the bare id we passed to render(), so sweeping that spelling too would
  // delete the diagram one line after inserting it — leaving an empty host.
  function sweepScratch(id) {
    var el = document.getElementById('d' + id);
    if (el && el.parentNode) el.parentNode.removeChild(el);
  }

  function showError(host, source, err) {
    host.classList.add('is-error');
    host.textContent = '';

    var box = document.createElement('div');
    box.className = 'mdr-mermaid-error';

    var head = document.createElement('div');
    head.className = 'mdr-mermaid-error-head';
    head.textContent = '⚠ Mermaid 图表渲染失败 — 图表源码里有语法错误';

    var msg = document.createElement('pre');
    msg.className = 'mdr-mermaid-error-msg';
    msg.textContent = String((err && (err.str || err.message)) || err || 'unknown error');

    var details = document.createElement('details');
    var summary = document.createElement('summary');
    summary.textContent = '查看图表源码';
    var src = document.createElement('pre');
    src.className = 'mdr-mermaid-error-src';
    src.textContent = source;
    details.appendChild(summary);
    details.appendChild(src);

    box.appendChild(head);
    box.appendChild(msg);
    box.appendChild(details);
    host.appendChild(box);
  }

  // Hand the finished diagram to the shared zoom/pan viewer. Clicking the
  // diagram (or pressing Enter on it) opens a full-screen overlay where the
  // user can scale it up and pan around — a 30-node flowchart squeezed into
  // the column is unreadable at 1:1.
  function decorate(svg) {
    svg.classList.add('mdr-diagram-svg');
    svg.setAttribute('role', 'button');
    svg.setAttribute('tabindex', '0');
    svg.setAttribute('title', '点击放大查看');
    if (!window.MDReaderViewer) return;
    svg.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation();
      window.MDReaderViewer.openElement(svg);
    });
    svg.addEventListener('keydown', function (e) {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        window.MDReaderViewer.openElement(svg);
      }
    });
  }

  function renderOne(host, seq) {
    var source = host.textContent;
    var id = 'mdr-mermaid-' + seq;
    var pending;
    try {
      pending = window.mermaid.render(id, source);
    } catch (err) {
      showError(host, source, err);
      sweepScratch(id);
      return Promise.resolve();
    }
    return Promise.resolve(pending)
      .then(function (result) {
        var svgText = result && typeof result.svg === 'string' ? result.svg : '';
        if (!svgText) {
          // Resolving without an SVG would leave a host that is neither a
          // diagram nor an error box — the one state this file promises never
          // to produce. Route it through the error path instead.
          throw new Error('mermaid resolved without an SVG');
        }
        host.innerHTML = svgText;
        var svg = host.querySelector('svg');
        if (svg) {
          // Let the theme width rule drive sizing in the inline view.
          svg.removeAttribute('height');
          decorate(svg);
        }
        if (typeof result.bindFunctions === 'function') result.bindFunctions(host);
        host.classList.add('is-rendered');
      })
      .catch(function (err) {
        showError(host, source, err);
      })
      .then(function () {
        sweepScratch(id);
      });
  }

  // Sequential on purpose: mermaid keeps shared config + measurement state,
  // so rendering several diagrams concurrently interleaves their scratch DOM.
  // The diagrams here are few and the render is a few ms each.
  function render() {
    if (!window.mermaid) {
      showBanner();
      return Promise.resolve();
    }
    // Skip hosts that already hold a result. This is what makes render()
    // idempotent, and it is load-bearing rather than defensive: "Export HTML"
    // serialises the *live* DOM via toHtml(), so an exported page arrives with
    // every diagram already rendered inside its host. Running mermaid over
    // those a second time would feed it the SVG's own <style> block as if it
    // were diagram source, and the parse failure would replace the finished
    // diagram with an error box.
    var hosts = document.querySelectorAll('.mdr-mermaid:not(.is-rendered):not(.is-error)');
    if (!hosts.length) return Promise.resolve();
    initialize();

    var chain = Promise.resolve();
    for (var i = 0; i < hosts.length; i++) {
      (function (host, seq) {
        chain = chain.then(function () { return renderOne(host, seq); });
      })(hosts[i], i + 1);
    }
    return chain;
  }

  function showBanner() {
    if (document.getElementById('mdr-mermaid-banner')) return;
    var banner = document.createElement('div');
    banner.id = 'mdr-mermaid-banner';
    banner.setAttribute('style',
      'position:fixed;top:0;left:0;right:0;background:#fdf6c4;color:#000;' +
      'padding:6px;text-align:center;font:12px sans-serif;z-index:2147483001;');
    banner.textContent = 'Mermaid 未能加载，图表无法绘制';
    document.body.appendChild(banner);
  }

  window.MDReaderMermaid = {
    convert: convert,
    render: render,
    // Exposed for the integration test: it drives the overlay without
    // synthesising trusted mouse events. The overlay itself now lives in
    // viewer-overlay.js — these are thin aliases kept for compatibility with
    // the existing test surface.
    _open: function (svg) { return window.MDReaderViewer && window.MDReaderViewer.openElement(svg); },
    _close: function () { return window.MDReaderViewer && window.MDReaderViewer.close(); },
    _isOpen: function () { return !!(window.MDReaderViewer && window.MDReaderViewer.isOpen()); },
  };
})();
