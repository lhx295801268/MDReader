#include "services/RenderCoordinator.h"
#include "services/WorkerThread.h"
#include "services/MarkdownRenderer.h"
#include <QTimer>

namespace {
MarkdownRenderer* renderer() {
    static MarkdownRenderer r;
    return &r;
}
}  // namespace

RenderCoordinator::RenderCoordinator(QObject* parent) : QObject(parent) {}

void RenderCoordinator::bind(const QString& docId, PreviewView* preview) {
    unbind(docId);  // 清理旧绑定,防止旧 QTimer 持续触发
    Pending p;
    p.preview = preview;
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
                                      const QString& theme) {
    auto it = pendings_.find(docId);
    if (it == pendings_.end()) return;
    it->markdown = markdown;
    it->theme = theme;
    it->frameId = nextFrameId_++;
    if (mode_ == Manual) return;
    it->timer->start();      // restart 250 ms
}

void RenderCoordinator::onTimerTimeout() {
    auto* t = qobject_cast<QTimer*>(sender());
    if (!t) return;
    for (auto it = pendings_.begin(); it != pendings_.end(); ++it) {
        if (it->timer != t) continue;
        const QString docId = it.key();
        const QPointer<PreviewView> preview = it->preview;
        const quint64 frameAtKickoff = it->frameId;
        const QString md = it->markdown;
        const QString theme = it->theme;
        // 在 worker 线程渲染;回到主线程前再次比对 frameId,落后就丢弃。
        // 用 QPointer 守护 coordinator 生命周期,防止在 worker bounce 期间
        // 析构后回到主线程对 dangling this 触发 UB。
        QPointer<RenderCoordinator> self = this;
        runOnWorker([docId, frameAtKickoff, md, theme, preview, self]() {
            QString html = renderer()->render(md, theme);
            postToMain([docId, frameAtKickoff, html, preview, self]() {
                if (!self) return;                          // coordinator 已析构
                auto it = self->pendings_.find(docId);
                if (it == self->pendings_.end()) return;    // 已 unbind
                if (it->frameId != frameAtKickoff) return;  // 过期帧,丢
                if (preview) preview->setMarkdownHtml(html);
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
