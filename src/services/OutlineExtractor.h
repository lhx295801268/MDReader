#pragma once
#include <QString>
#include <QList>

class OutlineExtractor {
public:
    struct Entry { int level; QString text; int lineNumber; QString slug; };
    [[nodiscard]] static QList<Entry> extract(const QString& md);
};
