#pragma once
#include <QString>
#include <mutex>

class MarkdownRenderer {
public:
    MarkdownRenderer();
    ~MarkdownRenderer();
    MarkdownRenderer(const MarkdownRenderer&) = delete;
    MarkdownRenderer& operator=(const MarkdownRenderer&) = delete;

    // Thread-safe: internal mutex serializes cmark-gfm calls.
    QString render(const QString& markdown, const QString& themeName);

private:
    QString wrapHtml(const QString& bodyHtml, const QString& themeName);

    std::mutex mtx_;  // cmark-gfm parser state is not thread-safe
};