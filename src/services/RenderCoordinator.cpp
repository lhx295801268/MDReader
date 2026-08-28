#include "services/RenderCoordinator.h"
#include "services/WorkerThread.h"
#include "services/MarkdownRenderer.h"
#include "services/OutlineExtractor.h"
#include "services/WordCounter.h"
#include "ui/DocumentTab.h"
#include <QTimer>

namespace {
MarkdownRenderer* renderer() {
    static MarkdownRenderer r;
    return &r;
}
}  // namespace

RenderCoordinator::RenderCoordinator(QObject* parent) : QObject(parent) {}

void RenderCoordinator::bind(const QString& docId, PreviewView* preview) {
    // Legacy shim — no tab means no outline/stats dispatch.
    bind(docId, preview, nullptr);
}

void RenderCoordinator::bind(const QString& docId, PreviewView* preview, DocumentTab* tab) {
    unbind(docId);  // 清理旧绑定,防止旧 QTimer 持续触发
    Pending p;
    p.preview = preview;
    p.tab = tab;
    p.timer = new QTimer(this);
    p.timer->setSingleShot(true);
    p.timer->setInterval(250);
    connect(p.timer, &QTimer::timeout, this, &RenderCoordinator::onTimerTimeout);
    // p.frameId 保持默认 0;requestRender 在 timer 触发前会先赋值。
    pendings_.insert(docId, p);
}

void RenderCoordinator::unbind(const QString& docId) {
    auto it = pendings_.find(docId);
    if (it == pendings_.end()) return;
    delete it->timer;
    pendings_.erase(it);
}

void RenderCoordinator::setMode(Mode m) { mode_ = m; }

void RenderCoordinator::requestRender(const QString& docId, const QString& markdown,
                                      const QString& theme, bool force) {
    auto it = pendings_.find(docId);
    if (it == pendings_.end()) return;
    it->markdown = markdown;
    it->theme = theme;
    it->frameId = nextFrameId_++;
    // Show the loading overlay only for "discrete" renders: the initial
    // render on file open, the manual Refresh button, external-change
    // reloads, and theme rerenders. Live keystroke updates (force=false)
    // skip the overlay so the user doesn't see a 250ms flash on every
    // character — instead the overlay stays hidden while the preview
    // smoothly updates in place. endLoading() is called from
    // PreviewView::setMarkdownHtml(), which fires on every successful
    // render regardless of how it was triggered.
    if (force && it->preview) it->preview->beginLoading();
    if (mode_ == Manual && !force) return;
    it->timer->start();      // restart 250 ms
}

void RenderCoordinator::onTimerTimeout() {
    auto* t = qobject_cast<QTimer*>(sender());
    if (!t) return;
    for (auto it = pendings_.begin(); it != pendings_.end(); ++it) {
        if (it->timer != t) continue;
        const QString docId = it.key();
        const QPointer<PreviewView> preview = it->preview;
        const QPointer<DocumentTab> tab = it->tab;
        const quint64 frameAtKickoff = it->frameId;
        const QString md = it->markdown;
        const QString theme = it->theme;
        // 在 worker 线程渲染;回到主线程前再次比对 frameId,落后就丢弃。
        // 用 QPointer 守护 coordinator 生命周期,防止在 worker bounce 期间
        // 析构后回到主线程对 dangling this 触发 UB。
        // Task 20: outline/stats 也是纯函数,一起放到 worker 上算;最终的
        // tab 回调仍走 postToMain 回到主线程。
        QPointer<RenderCoordinator> self = this;
        runOnWorker([docId, frameAtKickoff, md, theme, preview, tab, self]() {
            QString html = renderer()->render(md, theme);
            auto entries = OutlineExtractor::extract(md);
            auto stats = WordCounter::count(md);
            postToMain([docId, frameAtKickoff, html, entries, stats, preview, tab, self]() {
                if (!self) return;                          // coordinator 已析构
                auto it = self->pendings_.find(docId);
                if (it == self->pendings_.end()) return;    // 已 unbind
                if (it->frameId != frameAtKickoff) return;  // 过期帧,丢
                if (preview) preview->setMarkdownHtml(html);
                if (tab) tab->onContentUpdated(entries, stats);
                emit self->renderSucceeded(html);
            });
        });
        break;
    }
}

void RenderCoordinator::simulateLateDelivery(const QString& docId, quint64 frameId,
                                             bool stale, const QString& html) {
    Q_UNUSED(docId);
    Q_UNUSED(frameId);
    if (stale) return;
    emit renderSucceeded(html);
}
