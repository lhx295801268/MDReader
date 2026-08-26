#pragma once
#include <QString>

class WordCounter {
public:
    // Words: contiguous runs of [\w一-鿿]+ (includes underscores and basic CJK).
    // Chars: total QString length in UTF-16 code units (BMP characters count once,
    //        surrogate pairs count twice).
    // Paragraphs: non-empty content segments separated by blank lines; ATX heading
    //              lines are INCLUDED as their own segment.
    // Headings: ATX-style (# .. ######) only — no Setext, no fence filtering.
    struct Stats { int words = 0; int chars = 0; int paragraphs = 0; int headings = 0; };
    static Stats count(const QString& md);
};
