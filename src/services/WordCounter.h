#pragma once
#include <QString>

class WordCounter {
public:
    struct Stats { int words = 0; int chars = 0; int paragraphs = 0; int headings = 0; };
    static Stats count(const QString& md);
};
