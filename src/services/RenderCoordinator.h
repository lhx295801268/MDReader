#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QPointer>
#include "ui/PreviewView.h"

class QTimer;

class RenderCoordinator : public QObject {
    Q_OBJECT
public:
    enum Mode { Live, Manual };
    Q_ENUM(Mode)

    explicit RenderCoordinator(QObject* parent = nullptr);

    void bind(const QString& docId, PreviewView* preview);
    void unbind(const QString& docId);

    void requestRender(const QString& docId, const QString& markdown,
                       const QString& theme);

    void setMode(Mode m);
    Mode mode() const { return mode_; }

    // 测试辅助:模拟 worker 把"某帧的 HTML"投回主线程。
    void simulateLateDelivery(const QString& docId, quint64 frameId,
                              bool stale, const QString& html);

signals:
    void renderSucceeded(const QString& html);
    void renderFailed(const QString& message);

private slots:
    void onTimerTimeout();

private:
    struct Pending {
        QPointer<PreviewView> preview;
        QString markdown;
        QString theme;
        quint64 frameId = 0;
        QTimer* timer = nullptr;
    };

    Mode mode_ = Live;
    quint64 nextFrameId_ = 1;
    QHash<QString, Pending> pendings_;
};
