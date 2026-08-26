#pragma once
#include <QWebEngineView>
#include <QString>

class PreviewView : public QWebEngineView {
    Q_OBJECT
public:
    explicit PreviewView(QWidget* parent = nullptr);
    void setMarkdownHtml(const QString& fullHtml);
    void exportHtml(const QString& filePath, const QString& currentHtml);
    void exportPdf(const QString& filePath);
    virtual void onRenderFailed();
};