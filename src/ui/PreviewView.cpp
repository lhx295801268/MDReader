#include "ui/PreviewView.h"
#include <QFile>
#include <QPageLayout>
#include <QPageSize>
#include <QUrl>

PreviewView::PreviewView(QWidget* parent) : QWebEngineView(parent) {}

void PreviewView::setMarkdownHtml(const QString& fullHtml) {
    setHtml(fullHtml, QUrl(QStringLiteral("qrc:/")));
}

void PreviewView::exportHtml(const QString& filePath, const QString& currentHtml) {
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("PreviewView::exportHtml: cannot open %s: %s",
                 qUtf8Printable(filePath), qUtf8Printable(f.errorString()));
        return;
    }
    f.write(currentHtml.toUtf8());
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