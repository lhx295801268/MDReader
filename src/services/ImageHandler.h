#pragma once
#include <QString>
#include <QByteArray>

// Temporary stub created in Task 8 to satisfy EditorView.cpp's #include
// so the EditorView translation unit compiles. The .cpp is intentionally
// NOT created here (and not added to qt_add_executable) — link will fail
// with "undefined reference to ImageHandler::handle" until Task 9 lands
// the real implementation.
class ImageHandler {
public:
    static QString handle(const QByteArray& bytes,
                          const QString& docDir,
                          const QString& baseName);
};
