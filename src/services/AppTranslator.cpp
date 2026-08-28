#include "services/AppTranslator.h"
#include <QCoreApplication>

AppTranslator::AppTranslator(QObject* parent) : QTranslator(parent) {
    // Each entry maps the English source text (the literal passed to tr()
    // in the source code) to its Simplified Chinese rendering. Add new
    // strings here when you introduce a new tr() call so the toolbar /
    // dialogs stay consistent.
    //
    // Keys use byte-for-byte the same characters as the source code; Qt
    // passes tr() arguments as const char* and the lookup uses QByteArray
    // equality, so the comparison is encoding-stable (UTF-8 bytes).
    const struct Entry { const char* en; const char* zh; } table[] = {
        // Toolbar actions (MainWindow.cpp)
        {"New",            "\xe6\x96\xb0\xe5\xbb\xba"},  // 新建
        {"Open",           "\xe6\x89\x93\xe5\xbc\x80"},  // 打开
        {"Save",           "\xe4\xbf\x9d\xe5\xad\x98"},  // 保存
        {"Export PDF",     "\xe5\xaf\xbc\xe5\x87\xba PDF"},  // 导出 PDF
        {"Export HTML",    "\xe5\xaf\xbc\xe5\x87\xba HTML"}, // 导出 HTML
        {"Outline",        "\xe5\xa4\xa7\xe7\xba\xb2"},  // 大纲
        {"Info",           "\xe4\xbf\xa1\xe6\x81\xaf"},  // 信息
        {"Theme",          "\xe4\xb8\xbb\xe9\xa2\x98"},  // 主题
        {"Live",           "\xe5\xae\x9e\xe6\x97\xb6"},  // 实时
        {"Manual",         "\xe6\x89\x8b\xe5\x8a\xa8"},  // 手动
        {"Refresh",        "\xe5\x88\xb7\xe6\x96\xb0"},  // 刷新
        // Theme menu
        {"Follow System",  "\xe8\xb7\x9f\xe9\x9a\x8f\xe7\xb3\xbb\xe7\xbb\x9f"}, // 跟随系统
        // Theme basenames (display labels, internal ids stay in English)
        {"github",         "GitHub"},
        {"github-dark",    "GitHub Dark"},
        {"dracula",        "Dracula"},
        {"solarized-light","Solarized Light"},
        {"solarized-dark", "Solarized Dark"},
        {"one-dark",       "One Dark"},
        // Dialog strings (MainWindow.cpp)
        {"Reloaded from disk", "\xe4\xbb\x8e\xe7\x9b\x98\xe5\x8d\x95\xe9\x87\x8d\xe6\x96\xb0\xe5\x8a\xa0\xe8\xbd\xbd"}, // 从盘单重新加载
        {"File changed on disk: %1", "\xe7\x9b\x98\xe5\x8d\x95\xe4\xb8\x8a\xe7\x9a\x84\xe6\x96\x87\xe4\xbb\xb6\xe5\xb7\xb2\xe6\x94\xb9\xe5\x8f\x98\xef\xbc\x9a%1"}, // 盘单上的文件已改变: %1
        {"How do you want to handle it?", "\xe6\x82\xa8\xe6\x83\xb3\xe5\xa6\x82\xe4\xbd\x95\xe5\xa4\x84\xe7\x90\x86\xe5\xae\x83\xef\xbc\x9f"}, // 您想如何处理它?
        {"Discard my edits and reload", "\xe4\xb8\xa2\xe5\xbc\x83\xe6\x88\x91\xe7\x9a\x84\xe7\xbc\x96\xe8\xbe\x91\xe5\xb9\xb6\xe9\x87\x8d\xe6\x96\xb0\xe5\x8a\xa0\xe8\xbd\xbd"}, // 丢弃我的编辑并重新加载
        {"Keep my edits", "\xe4\xbf\x9d\xe7\x95\x99\xe6\x88\x91\xe7\x9a\x84\xe7\xbc\x96\xe8\xbe\x91"}, // 保留我的编辑
        {"Backup then reload", "\xe5\xa4\x87\xe4\xbb\xbd\xe5\x90\x8e\xe9\x87\x8d\xe6\x96\xb0\xe5\x8a\xa0\xe8\xbd\xbd"}, // 备份后重新加载
        {"Backup failed for %1", "\xe5\xa4\x87\xe4\xbb\xbd\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x9a%1"}, // 备份失败: %1
        {"Saved backup to %1", "\xe5\xb7\xb2\xe5\xb0\x86\xe5\xa4\x87\xe4\xbb\xbd\xe4\xbf\x9d\xe5\xad\x98\xe5\x88\xb0%1"}, // 已将备份保存到%1
        // File dialog filters
        {"Markdown (*.md *.markdown)", "Markdown (*.md *.markdown)"},
        {"Markdown (*.md)",            "Markdown (*.md)"},
        {"PDF (*.pdf)",                "PDF (*.pdf)"},
        {"HTML (*.html *.htm)",        "HTML (*.html *.htm)"},
        // Dialog titles
        {"Open MD",      "\xe6\x89\x93\xe5\xbc\x80 Markdown"}, // 打开 Markdown
        {"Save MD",      "\xe4\xbf\x9d\xe5\xad\x98 Markdown"}, // 保存 Markdown
        {"Export PDF",   "\xe5\xaf\xbc\xe5\x87\xba PDF"},
        {"Export HTML",  "\xe5\xaf\xbc\xe5\x87\xba HTML"},
        // Window title
        {"MDReader",     "MDReader"},
        // Language menu labels (used by the language switcher itself)
        {"Language",     "\xe8\xaf\xad\xe8\xa8\x80"},  // 语言
        {"English",      "English"},
        {"\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87", "简体中文"},
    };
    for (const auto& e : table) {
        zhTable_.insert(QByteArray(e.en), QByteArray(e.zh));
    }
}

void AppTranslator::setLanguage(Language lang) {
    if (lang_ == lang) return;
    lang_ = lang;
}

QString AppTranslator::languageLabel(Language lang) {
    switch (lang) {
        case English: return QStringLiteral("English");
        case Chinese: return QStringLiteral("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"); // 简体中文
    }
    return QString();
}

QString AppTranslator::translate(const char* /*context*/, const char* sourceText,
                                 const char* /*disambiguation*/, int n) const {
    if (!sourceText) return QString();
    if (lang_ != Chinese) return QString::fromUtf8(sourceText);
    // Look up the source text in the Chinese table. We use QByteArray
    // equality on the raw bytes because all entries above are UTF-8.
    QByteArray key(sourceText);
    auto it = zhTable_.constFind(key);
    if (it == zhTable_.constEnd()) {
        // No translation for this string — fall back to the source.
        // Returning sourceText keeps the app usable even when tr() calls
        // are added without a matching table entry.
        return QString::fromUtf8(sourceText);
    }
    QString out = QString::fromUtf8(*it);
    // Qt's %n plural handling — we don't have any plural forms in our
    // table, but pass it through so callers using tr("%n item", "", n)
    // don't crash. For singular (n == 1) we keep the string as-is; for
    // plural we leave it to Qt's normal handling (it'll use the source).
    if (n >= 0 && out.contains(QLatin1String("%n"))) {
        // Strip %n and let the caller format. We don't have explicit
        // plural forms so just substitute a numeric literal.
        out.replace(QLatin1String("%n"), QString::number(n));
    }
    return out;
}

void AppTranslator::retranslateRegistered() {
    // Iterate over all child QObjects and ask them to refresh their
    // visible strings. Most Qt widgets respond to QEvent::LanguageChange
    // by re-running their tr() calls automatically — but only if they
    // were created with Q_OBJECT and are parented under the QApplication.
    // We send the event directly to be sure.
    const auto all = QCoreApplication::instance()->children();
    for (QObject* o : all) {
        QCoreApplication::sendEvent(o, new QEvent(QEvent::LanguageChange));
    }
}