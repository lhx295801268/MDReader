#include "ui/PreviewView.h"
#include "services/HtmlInliner.h"
#include "ui/LoadingOverlay.h"
#include <QFile>
#include <QPageLayout>
#include <QPageSize>
#include <QUrl>
#include <QStandardPaths>
#include <QResizeEvent>

PreviewView::PreviewView(QWidget* parent) : QWebEngineView(parent) {
    // Construct the overlay as a child of the web view so it floats on
    // top of the page content. Hidden by default; RenderCoordinator flips
    // it on via beginLoading() and off via endLoading() around each
    // setMarkdownHtml() call.
    loading_ = new LoadingOverlay(this);
    loading_->hide();
}

void PreviewView::resizeEvent(QResizeEvent* e) {
    QWebEngineView::resizeEvent(e);
    // Keep the overlay covering the full preview area. The web view's
    // content rect is the same as its geometry (no scrollbars / chrome
    // outside the page rect).
    if (loading_) loading_->setGeometry(0, 0, width(), height());
}

void PreviewView::beginLoading() {
    if (!loading_) return;
    loading_->setGeometry(0, 0, width(), height());
    loading_->start();
    loading_->raise();
}

void PreviewView::endLoading() {
    if (!loading_) return;
    loading_->stop();
}

bool PreviewView::isLoading() const {
    return loading_ && loading_->isVisible();
}

void PreviewView::setMarkdownHtml(const QString& fullHtml) {
    setHtml(fullHtml, QUrl(QStringLiteral("qrc:/")));
    // Content is now in the web view; the page may still be loading its
    // resources (highlight.min.js, MathJax, theme CSS), but the overlay
    // is meant to mask the markdown→HTML render step (which is what the
    // user perceives as "loading"). Hiding it here gives instant feedback
    // that the render completed, even if MathJax is still typesetting.
    endLoading();
}

void PreviewView::exportHtml(const QString& filePath, const QString& currentHtml) {
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("PreviewView::exportHtml: cannot open %s: %s",
                 qUtf8Printable(filePath), qUtf8Printable(f.errorString()));
        return;
    }
    // qrc: URLs only resolve inside the app, so the live page's script tags
    // have to be inlined for the exported file to stand on its own.
    f.write(mdreader::html::inlineQrcScripts(currentHtml).toUtf8());
}

void PreviewView::exportPdf(const QString& filePath) {
    QPageLayout layout(QPageSize(QPageSize::A4),
                       QPageLayout::Portrait,
                       QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    page()->printToPdf(filePath, layout);
}

void PreviewView::onRenderFailed() {
    // 默认什么都不做;子类可重写以显示 banner。
    // 此处保留接口供后续主题/状态黄条挂接使用。
}