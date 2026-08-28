#pragma once
#include <QTranslator>
#include <QHash>

/// Self-contained in-app translator that ships its own English↔Chinese
/// mapping instead of relying on Qt's .ts / .qm / lupdate toolchain.
///
/// Rationale: we don't want to require lrelease / linguist at build time
/// or to ship separate .qm files. Two languages (English default, 简体中文)
/// are enough for this app, so we keep the table inline. Adding a third
/// language later means adding another QHash and a branch in translate().
///
/// Override behavior:
///   - translate(context, sourceText) returns the Chinese mapping when
///     currentLanguage() == Chinese, otherwise returns sourceText.
///   - translate(context, sourceText, disambiguation) honors the same
///     table; the disambiguation arg is unused because our table doesn't
///     have ambiguous entries.
///   - isEmpty() always returns false so Qt's load() machinery doesn't
///     reject us.
///   - load() is a no-op success — the data is built-in.
class AppTranslator : public QTranslator {
    Q_OBJECT
public:
    enum Language { English, Chinese };
    Q_ENUM(Language)

    explicit AppTranslator(QObject* parent = nullptr);

    Language currentLanguage() const { return lang_; }
    void setLanguage(Language lang);

    // Convenience: human-readable label for the language menu.
    static QString languageLabel(Language lang);

    // Re-translate every QObject that has been registered via
    // installOnObject(). Avoids the parent-app retranslateUi() because we
    // need a way to translate strings set with tr() across all our
    // widgets from a single switch.
    void retranslateRegistered();

protected:
    // QTranslator API. The "disambiguation" overload exists to match
    // Qt's virtual table — Qt picks the right one based on whether the
    // caller passed a third arg.
    QString translate(const char* context, const char* sourceText,
                      const char* disambiguation = nullptr,
                      int n = -1) const override;

    bool isEmpty() const override { return false; }

private:
    Language lang_ = English;
    // Source-text → Chinese translation. Keys are the raw char* bytes
    // passed to translate(). Built once in the ctor; no per-call
    // allocation.
    QHash<QByteArray, QByteArray> zhTable_;
};