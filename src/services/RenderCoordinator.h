#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QPointer>
#include "ui/PreviewView.h"

class QTimer;
class DocumentTab;

class RenderCoordinator : public QObject {
    Q_OBJECT
public:
    enum Mode { Live, Manual };
    Q_ENUM(Mode)

    explicit RenderCoordinator(QObject* parent = nullptr);

    // Legacy overload: callers that don't need outline/stats refresh
    // (e.g. tests) can still bind without a tab.
    void bind(const QString& docId, PreviewView* preview);
    // Task 20: full overload. Stores `tab` so the worker → main bounce can
    // dispatch the freshly-computed outline entries + word stats back to
    // the DocumentTab. Tab is held via QPointer and only ever touched on
    // the main thread, so no locking is needed.
    void bind(const QString& docId, PreviewView* preview, DocumentTab* tab);
    void unbind(const QString& docId);

    void requestRender(const QString& docId, const QString& markdown,
                       const QString& theme, bool force = false);

    void setMode(Mode m);
    Mode mode() const { return mode_; }

    // 测试辅助:模拟 worker 把"某帧的 HTML"投回主线程。
    void simulateLateDelivery(const QString& docId, quint64 frameId,
                              bool stale, const QString& html);

signals:
    void renderSucceeded(const QString& html);
    void renderFailed(const QString& message);  // Phase 3 will emit on MarkdownRenderer error

private slots:
    void onTimerTimeout();

private:
    struct Pending {
        QPointer<PreviewView> preview;
        QPointer<DocumentTab> tab;  // Task 20: nullptr for legacy bind()
        QString markdown;
        QString theme;
        quint64 frameId = 0;
        QTimer* timer = nullptr;
    };

    Mode mode_ = Live;
    quint64 nextFrameId_ = 1;
    QHash<QString, Pending> pendings_;
};
