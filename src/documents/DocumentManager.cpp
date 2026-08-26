#include "documents/DocumentManager.h"
#include "documents/Document.h"
#include "services/WorkerThread.h"
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTextStream>

DocumentManager::DocumentManager(QObject* parent) : QObject(parent) {}

void DocumentManager::openFile(const QString& path) {
    QPointer<DocumentManager> self = this;
    (void)QtConcurrent::run(QThreadPool::globalInstance(), [self, path] {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning("DocumentManager::openFile: cannot open %s: %s",
                     qUtf8Printable(path), qUtf8Printable(f.errorString()));
            return;
        }
        QTextStream in(&f); in.setEncoding(QStringConverter::Utf8);
        const QString content = in.readAll();
        QFileInfo fi(path);
        postToMain([self, fi, content]() mutable {
            if (!self) return;   // DocumentManager destroyed mid-flight
            auto doc = std::make_shared<Document>(fi.absoluteFilePath(), content);
            self->docs_ << doc;
            emit self->documentAdded(doc);
            emit self->documentLoaded(doc);
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
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning("DocumentManager::saveDocument: cannot open %s: %s",
                 qUtf8Printable(doc->path()), qUtf8Printable(f.errorString()));
        return false;
    }
    QTextStream out(&f); out.setEncoding(QStringConverter::Utf8);
    out << doc->text();
    doc->markSaved();
    return true;
}

void DocumentManager::closeDocument(std::shared_ptr<Document> doc) {
    docs_.removeAll(doc);
    emit documentClosed(doc);
}