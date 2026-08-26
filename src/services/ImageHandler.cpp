#include "services/ImageHandler.h"
#include <QCryptographicHash>
#include <QFile>
#include <QDir>

QString ImageHandler::handle(const QByteArray& bytes, const QString& docDir,
                             const QString& docBasename) {
    const QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    QString hex = QString::fromLatin1(hash.toHex().left(16));   // 截短足够
    QString assetsDir = docDir + "/" + docBasename + ".assets";
    QDir().mkpath(assetsDir);
    QString filePath = assetsDir + "/" + hex + ".png";
    if (!QFile::exists(filePath)) {
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning("ImageHandler::handle: cannot open %s: %s",
                     qUtf8Printable(filePath), qUtf8Printable(f.errorString()));
            return {};
        }
        if (f.write(bytes) != bytes.size()) {
            qWarning("ImageHandler::handle: short write to %s",
                     qUtf8Printable(filePath));
            return {};
        }
    }
    QString relPath = docBasename + ".assets/" + hex + ".png";
    return relPath;
}
