#include "services/RenderCoordinator.h"
#include "services/WorkerThread.h"
#include "services/MarkdownRenderer.h"
#include <QCoreApplication>
#include <QTimer>

namespace {
MarkdownRenderer* renderer() {
    static MarkdownRenderer r;
    return &r;
}
}  // namespace

RenderCoordinator::RenderCoordinator(QObject* parent) : QObject(parent) {}

void RenderCoordinator::bind(const QString& docId, PreviewView* preview) {
    Pending p;
    p.preview = preview;
    p.timer = new QTimer(this);
    p.timer->setSingleShot(true);
    p.timer->setInterval(250);
    connect(p.timer, &QTimer::timeout, this, &RenderCoordinator::onTimerTimeout);
    p.frameId = nextFrameId_++;
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
        runOnWorker([this, docId, frameAtKickoff, md, theme, preview]() {
            QString html = renderer()->render(md, theme);
            postToMain([this, docId, frameAtKickoff, html, preview]() {
                auto it = pendings_.find(docId);
                if (it == pendings_.end()) return;          // 已 unbind
                if (it->frameId != frameAtKickoff) return;  // 过期帧,丢
                if (preview) preview->setMarkdownHtml(html);
                emit renderSucceeded(html);
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
