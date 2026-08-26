#include "ui/PreviewView.h"
#include <QFile>
#include <QPageLayout>
#include <QPageSize>

PreviewView::PreviewView(QWidget* parent) : QWebEngineView(parent) {}

void PreviewView::setMarkdownHtml(const QString& fullHtml) {
    setHtml(fullHtml, QUrl(QStringLiteral("qrc:///")));
}

void PreviewView::exportHtml(const QString& filePath, const QString& currentHtml) {
    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(currentHtml.toUtf8());
    }
}

void PreviewView::exportPdf(const QString& filePath) {
    QPageLayout layout(QPageSize(QPageSize::A4),
                       QPageLayout::Portrait,
                       QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    page()->printToPdf(filePath, layout);
}

void PreviewView::onRenderFailed() {
    // 默认什么都不做;子类可重写以显示 banner。
    // 此处保留接口给后续 Task 20 主题黄条挂接。
}