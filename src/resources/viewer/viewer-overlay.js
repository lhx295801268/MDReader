/* viewer-overlay.js — 预览页里的「大图 / 图表」查看器。
 *
 * 需求背景:文档里的流程图、时序图、架构图以及图片,一旦内容过多(节点多、
 * 分辨率高),在正文里被压缩到栏宽之后细节就完全看不清了。这个模块把
 * 「放大 / 缩小 / 拖动平移」做成一个全屏遮罩层,任何可放大的内容
 * (Mermaid 渲染出来的 <svg>、正文里的 <img>)点一下就能进来细看。
 *
 * 与宿主页面的契约(见 MarkdownRenderer::wrapHtml):
 *   - 这个文件总是被注入,和 Mermaid 无关 —— 一篇没有图表的文档里也可能有
 *     一张分辨率很高的截图。
 *   - 对外只暴露一个全局对象 window.MDReaderViewer:
 *       openElement(el)   打开任意 <img> 或 <svg>
 *       close() / isOpen()
 *       zoomIn() / zoomOut() / fit() / actualSize()
 *       getScale()         (测试用)
 *   - 交互:滚轮缩放(以光标为锚点)、按住拖动平移、双击适应窗口、Esc 关闭、
 *     工具栏按钮(放大 / 缩小 / 适应窗口 / 1:1 / 关闭)、键盘 + - 0 1。
 *
 * 全部逻辑放在页面里而不是 C++ 侧:缩放/平移本质上是 DOM 变换,放这里可以
 * 保持 MarkdownRenderer 仍然是一个纯 markdown→HTML 的函数。
 */
(function () {
  'use strict';

  // 放大上限给到 20x:大流程图的节点文字在 2~3x 就已经能看清,但架构图里
  // 的小字注释可能要拉到 10x 以上才不至于糊成一团。
  var MIN_SCALE = 0.05;
  var MAX_SCALE = 20;
  var WHEEL_ZOOM_RATE = 0.0015;
  var ZOOM_STEP = 1.25;
  var FIT_MARGIN = 48;

  var overlay = null;
  var stage = null;
  var toolbar = null;
  var scaleLabel = null;
  var hint = null;
  var view = { scale: 1, tx: 0, ty: 0 };
  var content = null;            // 当前展示的克隆节点
  var contentSize = { w: 800, h: 600 };
  var drag = null;
  var dragged = false;

  // ---------------------------------------------------------------- styles

  // 遮罩层的样式随主题走吗?不跟。它是一个独立的「看图」场景,固定用深色
  // 背景 + 浅色控件,在浅色主题和深色主题下都保持一致的可读性。
  var CSS = [
    '.mdr-diagram-overlay { position: fixed; inset: 0; z-index: 2147483000;',
    '  display: none; background: rgba(0,0,0,.82); overflow: hidden; }',
    '.mdr-diagram-overlay.is-open { display: block; }',
    '.mdr-diagram-overlay.is-dragging { cursor: grabbing; }',
    '.mdr-ov-stage { position: absolute; left: 0; top: 0;',
    '  transform-origin: 0 0; will-change: transform; }',
    '.mdr-ov-stage > svg, .mdr-ov-stage > img { display: block;',
    '  max-width: none; max-height: none; }',
    '.mdr-ov-toolbar { position: absolute; left: 50%; top: 16px;',
    '  transform: translateX(-50%); display: flex; align-items: center; gap: 6px;',
    '  padding: 6px 10px; border-radius: 10px;',
    '  background: rgba(28,28,30,.92); border: 1px solid rgba(255,255,255,.14);',
    '  box-shadow: 0 6px 24px rgba(0,0,0,.45); z-index: 2;',
    '  font: 13px/1.2 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }',
    '.mdr-ov-btn { appearance: none; border: 1px solid rgba(255,255,255,.18);',
    '  background: rgba(255,255,255,.08); color: #ececec; cursor: pointer;',
    '  border-radius: 6px; padding: 5px 10px; min-width: 32px;',
    '  font: inherit; line-height: 1.1; }',
    '.mdr-ov-btn:hover { background: rgba(255,255,255,.18); }',
    '.mdr-ov-btn:active { background: rgba(255,255,255,.28); }',
    '.mdr-ov-scale { color: #cfcfcf; min-width: 52px; text-align: center;',
    '  font-variant-numeric: tabular-nums; }',
    '.mdr-ov-hint { position: absolute; left: 50%; bottom: 18px;',
    '  transform: translateX(-50%); padding: 6px 14px; border-radius: 999px;',
    '  background: rgba(0,0,0,.6); color: #e6e6e6; pointer-events: none;',
    '  font: 12px/1.4 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;',
    '  white-space: nowrap; }',
    'html.mdr-ov-lock, html.mdr-ov-lock body { overflow: hidden; }',
    // 正文里可放大的内容:给出「可以点」的视觉暗示。
    '.mdr-zoomable { cursor: zoom-in; }',
    '.mdr-diagram-svg { max-width: 100%; height: auto; }',
    '.mdr-diagram-svg:focus { outline: 2px solid #58a6ff; outline-offset: 4px; }',
    '@media print { .mdr-diagram-overlay { display: none !important; } }',
  ].join('\n');

  function injectStyle() {
    if (document.getElementById('mdr-viewer-style')) return;
    var style = document.createElement('style');
    style.id = 'mdr-viewer-style';
    style.textContent = CSS;
    (document.head || document.documentElement).appendChild(style);
  }

  // --------------------------------------------------------------- helpers

  function clampScale(scale) {
    return Math.min(MAX_SCALE, Math.max(MIN_SCALE, scale));
  }

  function tagOf(el) {
    return el && el.tagName ? el.tagName.toLowerCase() : '';
  }

  // SVG 的「原始尺寸」取自 viewBox —— mermaid 渲染出来的图没有 width/height
  // 属性(它们被 renderOne() 摘掉了),只有 viewBox 说了算。
  function svgNaturalSize(svg) {
    var vb = svg.getAttribute('viewBox');
    if (vb) {
      var parts = vb.split(/[\s,]+/).map(Number);
      if (parts.length === 4 && parts[2] > 0 && parts[3] > 0) {
        return { w: parts[2], h: parts[3] };
      }
    }
    var rect = svg.getBoundingClientRect();
    return { w: rect.width || 800, h: rect.height || 600 };
  }

  function imageNaturalSize(img) {
    // naturalWidth/naturalHeight 是图片的解码后像素尺寸,正是「放大到
    // 100% 想要多大」的那个尺寸。图片还没加载完时退回到布局尺寸。
    if (img.naturalWidth && img.naturalHeight) {
      return { w: img.naturalWidth, h: img.naturalHeight };
    }
    return { w: img.width || 800, h: img.height || 600 };
  }

  function updateScaleLabel() {
    if (!scaleLabel) return;
    scaleLabel.textContent = Math.round(view.scale * 100) + '%';
  }

  function applyView() {
    if (!stage) return;
    stage.style.transform =
      'translate(' + view.tx + 'px,' + view.ty + 'px) scale(' + view.scale + ')';
    updateScaleLabel();
  }

  // 以 (cx, cy) 为锚点缩放:锚点下的内容保持不动,这样滚轮放大时不会
  // 「内容从眼前跑掉」。
  function zoomTo(next, cx, cy) {
    var scale = clampScale(next);
    if (scale === view.scale) return;
    var rect = overlay.getBoundingClientRect();
    var px = (cx === undefined) ? rect.width / 2 : (cx - rect.left);
    var py = (cy === undefined) ? rect.height / 2 : (cy - rect.top);
    var ratio = scale / view.scale;
    view.tx = px - (px - view.tx) * ratio;
    view.ty = py - (py - view.ty) * ratio;
    view.scale = scale;
    applyView();
  }

  function zoomIn()  { zoomTo(view.scale * ZOOM_STEP); }
  function zoomOut() { zoomTo(view.scale / ZOOM_STEP); }

  // 适应窗口:整张图完整可见,但绝不放大到超过 1:1 —— 放大的位图只会变糊。
  function fit() {
    if (!content) return;
    var rect = overlay.getBoundingClientRect();
    var vw = rect.width || window.innerWidth;
    var vh = rect.height || window.innerHeight;
    var scale = Math.min((vw - FIT_MARGIN * 2) / contentSize.w,
                         (vh - FIT_MARGIN * 2) / contentSize.h, 1);
    view.scale = clampScale(scale);
    view.tx = (vw - contentSize.w * view.scale) / 2;
    view.ty = (vh - contentSize.h * view.scale) / 2;
    applyView();
  }

  // 1:1:按图片的原始像素显示。位图在这里是最清晰的,矢量图则是其设计尺寸。
  function actualSize() {
    if (!content) return;
    var rect = overlay.getBoundingClientRect();
    view.scale = 1;
    view.tx = (rect.width - contentSize.w) / 2;
    view.ty = (rect.height - contentSize.h) / 2;
    applyView();
  }

  // ----------------------------------------------------------------- open

  function ensureOverlay() {
    if (overlay) return;
    injectStyle();

    overlay = document.createElement('div');
    overlay.className = 'mdr-diagram-overlay';
    overlay.setAttribute('role', 'dialog');
    overlay.setAttribute('aria-modal', 'true');

    stage = document.createElement('div');
    stage.className = 'mdr-ov-stage';

    toolbar = document.createElement('div');
    toolbar.className = 'mdr-ov-toolbar';
    toolbar.appendChild(makeButton('＋', '放大 (Ctrl/Cmd +)', function () { zoomIn(); }));
    toolbar.appendChild(makeButton('－', '缩小 (Ctrl/Cmd -)', function () { zoomOut(); }));
    toolbar.appendChild(makeButton('适应窗口', '适应窗口 (双击 / 0)', function () { fit(); }));
    toolbar.appendChild(makeButton('1:1', '原始大小 (1)', function () { actualSize(); }));
    scaleLabel = document.createElement('span');
    scaleLabel.className = 'mdr-ov-scale';
    toolbar.appendChild(scaleLabel);
    toolbar.appendChild(makeButton('✕', '关闭 (Esc)', function () { close(); }));

    hint = document.createElement('div');
    hint.className = 'mdr-ov-hint';
    hint.textContent = '滚轮缩放 · 拖动平移 · 双击适应窗口 · Esc 关闭';

    overlay.appendChild(stage);
    overlay.appendChild(toolbar);
    overlay.appendChild(hint);
    document.body.appendChild(overlay);

    overlay.addEventListener('mousedown', onDragStart);
    overlay.addEventListener('wheel', onWheel, { passive: false });
    overlay.addEventListener('dblclick', onDblClick);
    overlay.addEventListener('click', onBackdropClick);
    document.addEventListener('keydown', onKeyDown);
    window.addEventListener('resize', function () { if (isOpen()) fit(); });
  }

  function makeButton(text, title, handler) {
    var btn = document.createElement('button');
    btn.className = 'mdr-ov-btn';
    btn.type = 'button';
    btn.textContent = text;
    btn.title = title;
    btn.setAttribute('aria-label', title);
    btn.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation();
      handler();
    });
    return btn;
  }

  function isOpen() {
    return !!overlay && overlay.classList.contains('is-open');
  }

  // 打开任意一个可放大的节点。<img> 和 <svg> 走不同的尺寸来源,但对外
  // 表现一致:一个居中的、可缩放可拖动的画面。
  function openElement(el) {
    if (!el) return false;
    var kind = tagOf(el);
    if (kind !== 'img' && kind !== 'svg') return false;

    ensureOverlay();

    var clone = el.cloneNode(true);
    clone.removeAttribute('id');
    clone.removeAttribute('style');

    if (kind === 'img') {
      contentSize = imageNaturalSize(el);
      clone.setAttribute('width', contentSize.w);
      clone.setAttribute('height', contentSize.h);
      clone.style.maxWidth = 'none';
      clone.style.display = 'block';
    } else {
      contentSize = svgNaturalSize(el);
      clone.setAttribute('width', contentSize.w);
      clone.setAttribute('height', contentSize.h);
      clone.style.maxWidth = 'none';
      clone.style.display = 'block';
      // 图表保留它被设计时的背景色:浅色图表放在深色遮罩上会变成一团黑,
      // 文字完全读不出来。
      var bodyBg = getComputedStyle(document.body).backgroundColor;
      clone.style.background =
        (bodyBg && bodyBg !== 'rgba(0, 0, 0, 0)') ? bodyBg : '#ffffff';
    }

    while (stage.firstChild) stage.removeChild(stage.firstChild);
    stage.appendChild(clone);
    content = clone;

    overlay.classList.add('is-open');
    document.documentElement.classList.add('mdr-ov-lock');
    fit();
    return true;
  }

  function close() {
    if (!overlay) return;
    overlay.classList.remove('is-open', 'is-dragging');
    document.documentElement.classList.remove('mdr-ov-lock');
    while (stage.firstChild) stage.removeChild(stage.firstChild);
    content = null;
    drag = null;
    dragged = false;
  }

  // -------------------------------------------------------------- 交互事件

  function onWheel(e) {
    if (!content) return;
    e.preventDefault();
    var next = clampScale(view.scale * Math.exp(-e.deltaY * WHEEL_ZOOM_RATE));
    zoomTo(next, e.clientX, e.clientY);
  }

  function onDragStart(e) {
    if (!content || e.button !== 0) return;
    // 工具栏上的按钮自己处理点击,不要顺手把遮罩拖走。
    if (e.target && e.target.closest && e.target.closest('.mdr-ov-toolbar')) return;
    e.preventDefault();
    drag = { x: e.clientX, y: e.clientY, tx: view.tx, ty: view.ty };
    dragged = false;
    overlay.classList.add('is-dragging');
    window.addEventListener('mousemove', onDragMove);
    window.addEventListener('mouseup', onDragEnd);
  }

  function onDragMove(e) {
    if (!drag) return;
    var dx = e.clientX - drag.x;
    var dy = e.clientY - drag.y;
    if (Math.abs(dx) > 3 || Math.abs(dy) > 3) dragged = true;
    view.tx = drag.tx + dx;
    view.ty = drag.ty + dy;
    applyView();
  }

  function onDragEnd() {
    drag = null;
    if (overlay) overlay.classList.remove('is-dragging');
    window.removeEventListener('mousemove', onDragMove);
    window.removeEventListener('mouseup', onDragEnd);
  }

  function onDblClick(e) {
    if (!content) return;
    if (e.target && e.target.closest && e.target.closest('.mdr-ov-toolbar')) return;
    e.preventDefault();
    fit();
  }

  function onBackdropClick(e) {
    // 拖动结束时浏览器也会补一个 click,只有「干净的一击」才关闭遮罩。
    if (e.target === overlay && !dragged) close();
  }

  function onKeyDown(e) {
    if (!isOpen()) return;
    if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      close();
      return;
    }
    // 工具栏按钮获得焦点时,回车/空格应该触发按钮而不是被这里吞掉。
    var tag = tagOf(e.target);
    if (tag === 'button' || tag === 'input' || tag === 'textarea') return;

    if (e.key === '+' || e.key === '=') { e.preventDefault(); zoomIn(); }
    else if (e.key === '-' || e.key === '_') { e.preventDefault(); zoomOut(); }
    else if (e.key === '0') { e.preventDefault(); fit(); }
    else if (e.key === '1') { e.preventDefault(); actualSize(); }
  }

  window.MDReaderViewer = {
    openElement: openElement,
    close: close,
    isOpen: isOpen,
    zoomIn: zoomIn,
    zoomOut: zoomOut,
    fit: fit,
    actualSize: actualSize,
    // 测试用:读出当前缩放比和位移,断言缩放/拖拽真的生效。
    getScale: function () { return view.scale; },
    getOffset: function () { return { tx: view.tx, ty: view.ty }; },
    getContentSize: function () { return { w: contentSize.w, h: contentSize.h }; },
  };
})();
