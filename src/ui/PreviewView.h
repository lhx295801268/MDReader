#pragma once
#include <QWebEngineView>
#include <QString>

class QLabel;
class LoadingOverlay;

class PreviewView : public QWebEngineView {
    Q_OBJECT
public:
    explicit PreviewView(QWidget* parent = nullptr);
    void setMarkdownHtml(const QString& fullHtml);
    void exportHtml(const QString& filePath, const QString& currentHtml);
    void exportPdf(const QString& filePath);
    virtual void onRenderFailed();

    // Show / hide the loading overlay that covers the preview while a
    // render is in flight (between requestRender and the postToMain
    // callback that calls setMarkdownHtml). Calling beginLoading() while
    // already loading is a no-op; endLoading() without a matching
    // beginLoading() is also a no-op (we don't pulse on every render —
    // the overlay stays until content actually arrives).
    void beginLoading();
    void endLoading();
    bool isLoading() const;

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    LoadingOverlay* loading_ = nullptr;
};