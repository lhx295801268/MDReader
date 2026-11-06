/* preview-zoom.js — 把正文里的图片接上查看器。
 *
 * Markdown 里的图片(`![说明](路径)`)在正文里总是被压缩到栏宽以内,分辨率
 * 高的截图/示意图因此糊成一团,细节完全读不出来。这个文件给每一张图片加上
 * 「点击放大」的能力:点一下就进入 viewer-overlay.js 提供的全屏查看器,
 * 之后可以滚轮缩放、拖动平移。
 *
 * 与宿主页面的契约:
 *   - viewer-overlay.js 总是被注入,本文件也就总是被注入 —— 一篇没有图表
 *     的文档里也可能有需要放大的截图。
 *   - 依赖 window.MDReaderViewer;它不在时本文件什么也不做(不报错),
 *     这样单独在浏览器里打开导出的 HTML 也不会炸掉。
 *   - 导出的 HTML 里这两个脚本会被内联进页面,行为与在应用里一致。
 */
(function () {
  'use strict';

  // 已经接管过的图片打个标记:页面每次重新渲染(实时预览模式下的去抖刷新)
  // 都会重建 DOM,标记能保证同一张图不会被绑定两次。
  var MARK = 'data-mdr-zoom';

  function enhance() {
    if (!window.MDReaderViewer) return;
    var imgs = document.querySelectorAll('img');
    for (var i = 0; i < imgs.length; i++) {
      var img = imgs[i];
      if (img.getAttribute(MARK) === '1') continue;
      img.setAttribute(MARK, '1');
      img.classList.add('mdr-zoomable');
      if (!img.getAttribute('title')) {
        img.setAttribute('title', '点击放大查看');
      }
      img.addEventListener('click', onImageClick);
    }
  }

  function onImageClick(e) {
    // 带修饰键的点击留给浏览器:Cmd/Ctrl+点击链接图片应当照常打开链接,
    // 而不是被查看器拦下来。
    if (e.ctrlKey || e.metaKey || e.shiftKey || e.altKey) return;
    e.preventDefault();
    e.stopPropagation();
    window.MDReaderViewer.openElement(this);
  }

  function whenReady(fn) {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', fn);
    } else {
      fn();
    }
  }

  whenReady(enhance);
  // MathJax / Mermaid 是在 DOMContentLoaded 之后才把内容插进来的,再扫一遍
  // 保证后插入的图片同样可以放大。
  window.addEventListener('load', enhance);

   window.MDReaderPreviewZoom = { enhance: enhance };
})();
