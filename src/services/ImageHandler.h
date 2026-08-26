#pragma once
#include <QString>
#include <QByteArray>

class ImageHandler {
public:
    // 给定字节;写到 <docDir>/<basename>.assets/<sha256-truncated>.png;
    // 返回相对 docDir 的 markdown 引用路径(以子目录起头,不含 "./")。
    static QString handle(const QByteArray& bytes, const QString& docDir,
                          const QString& docBasename);
};
