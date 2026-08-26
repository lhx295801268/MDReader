#include "documents/DocumentManager.h"
#include "documents/Document.h"
#include "services/WorkerThread.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

DocumentManager::DocumentManager(QObject* parent) : QObject(parent) {}

void DocumentManager::openFile(const QString& path) {
    QtConcurrent::run(QThreadPool::globalInstance(), [this, path] {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QTextStream in(&f); in.setEncoding(QStringConverter::Utf8);
        const QString content = in.readAll();
        QFileInfo fi(path);
        postToMain([this, content, path, fi]() mutable {
            auto doc = std::make_shared<Document>(fi.absoluteFilePath(), content);
            docs_ << doc;
            emit documentAdded(doc);
            emit documentLoaded(doc);
        });
    });
}

std::shared_ptr<Document> DocumentManager::addNew(const QString& initialText) {
    auto doc = std::make_shared<Document>(QString(), initialText);
    docs_ << doc;
    emit documentAdded(doc);
    return doc;
}

bool DocumentManager::saveDocument(std::shared_ptr<Document> doc) {
    if (!doc) return false;
    QFile f(doc->path());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return false;
    QTextStream out(&f); out.setEncoding(QStringConverter::Utf8);
    out << doc->text();
    doc->markSaved();
    return true;
}

void DocumentManager::closeDocument(std::shared_ptr<Document> doc) {
    docs_.removeAll(doc);
    emit documentClosed(doc);
}