#include "ui/LoadingOverlay.h"
#include <QPainter>
#include <QPalette>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QPaintEvent>
#include <QResizeEvent>

namespace {
constexpr int kSpinTickMs = 80;
}

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    // Translucent: leave a hint of the previous content visible so the
    // user knows the preview is still there, just updating. We don't
    // autoFillBackground because the paintEvent draws the dim layer
    // explicitly (alpha < 255 so QPainter::fillRect with a color alpha
    // works correctly across style engines).
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    caption_ = new QLabel(tr("Loading..."), this);
    caption_->setAlignment(Qt::AlignCenter);
    // Subtle color via palette so light/dark themes both look right
    // without hardcoding hex values. The arc below uses the same color.
    QPalette p = caption_->palette();
    p.setColor(QPalette::WindowText, p.color(QPalette::WindowText));
    caption_->setPalette(p);
    // The label sits BELOW the painted arc; lay it out centered in the
    // overlay. The arc itself is drawn in paintEvent around the label's
    // center point.
    lay->addStretch();
    lay->addWidget(caption_, 0, Qt::AlignCenter);
    lay->addStretch();

    spinTimer_ = new QTimer(this);
    spinTimer_->setInterval(kSpinTickMs);
    connect(spinTimer_, &QTimer::timeout, this, [this]() {
        angle_ = (angle_ + 30) % 360;  // 360 / 30 = 12 ticks per full rev
        update();
    });
}

void LoadingOverlay::start() {
    if (spinTimer_->isActive()) return;
    show();
    raise();
    spinTimer_->start();
}

void LoadingOverlay::stop() {
    spinTimer_->stop();
    hide();
}

void LoadingOverlay::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    // Trigger a repaint so the arc stays centered after the preview
    // splitter resizes.
    update();
}

void LoadingOverlay::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Dim layer. Use ~50% alpha so the user sees "something is still
    // underneath, just refreshing" rather than a hard blank. The color
    // follows the palette background so light themes get a light wash
    // and dark themes get a dark wash.
    const QColor bg = palette().color(QPalette::Window);
    QColor dim = bg;
    dim.setAlphaF(0.55);
    p.fillRect(rect(), dim);

    // Arc: a 6-segment dashed circle around the label. Drawn in the
    // WindowText color so it matches the caption automatically.
    const QColor arc = palette().color(QPalette::WindowText);
    p.setPen(QPen(arc, 4, Qt::SolidLine, Qt::RoundCap));
    p.setOpacity(0.85);

    // Find the visual center of the caption (which sits in the middle
    // row of the vertical layout). Fall back to widget center if the
    // label hasn't been laid out yet.
    QPointF center = caption_ && caption_->isVisible()
                         ? QPointF(caption_->geometry().center())
                         : QPointF(width() / 2.0, height() / 2.0);
    const qreal radius = 18.0;

    // Draw 6 arc segments with gaps so it looks like a spinner rather
    // than a solid ring. Each segment spans 30deg; 12 segments * 30deg
    // = 360deg. We offset each by angle_ so the whole pattern rotates.
    constexpr int kSegmentSpan = 30;
    constexpr int kSegmentCount = 6;
    for (int i = 0; i < kSegmentCount; ++i) {
        const int start = (angle_ + i * (360 / kSegmentCount)) % 360;
        // Negative startAngle because Qt's drawArc measures CW from 3 o'clock
        // and we want to start at 12 o'clock for a more "loading"-ish look.
        p.drawArc(static_cast<int>(center.x() - radius),
                  static_cast<int>(center.y() - radius),
                  static_cast<int>(radius * 2), static_cast<int>(radius * 2),
                  -start * 16, -kSegmentSpan * 16);
    }
}