#pragma once
#include <QWidget>

class QLabel;
class QTimer;

/// Translucent overlay widget shown on top of the preview while a render
/// is in flight. Paints a centered rotating arc + "Loading..." text using
/// QPainter primitives so it requires no external asset and inherits the
/// app's palette (text + arc colors flip automatically between light and
/// dark themes via QPalette::WindowText). Auto-resizes to fill its parent
/// via the resizeEvent path PreviewView forwards.
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    // Rotation period in milliseconds. Faster than ~60ms looks frantic;
    // slower than ~150ms feels sluggish. 80ms is a good middle ground.
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    QLabel* caption_ = nullptr;
    QTimer* spinTimer_ = nullptr;
    int angle_ = 0;  // current rotation, 0..360, advances every spinTickMs
};