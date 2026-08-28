#include "documents/DocumentManager.h"
#include "documents/Document.h"
#include "services/WorkerThread.h"
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTextStream>

DocumentManager::DocumentManager(QObject* parent) : QObject(parent) {}

void DocumentManager::openFile(const QString& path) {
    // Cheap main-thread dedup: if a doc with this absolute path is already
    // loaded, do nothing. The caller (MainWindow) is also expected to check
    // via findByPath() BEFORE this and focus the existing tab — but we
    // re-check here so direct callers (drag‑drop, Open dialog) that don't
    // pre‑screen still avoid spawning a duplicate worker read + tab.
    if (!path.isEmpty()) {
        QFileInfo fi(path);
        const QString abs = fi.absoluteFilePath();
        for (const auto& d : docs_) {
            if (d && d->path() == abs) return;
        }
    }
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
            // Re-check inside the bounce: another openFile() for the same
            // path could have landed between our main-thread check above
            // and this postToMain callback (the worker read takes time).
            const QString abs = fi.absoluteFilePath();
            for (const auto& d : self->docs_) {
                if (d && d->path() == abs) return;
            }
            auto doc = std::make_shared<Document>(abs, content);
            self->docs_ << doc;
            emit self->documentAdded(doc);
            emit self->documentLoaded(doc);
        });
    });
}

std::shared_ptr<Document> DocumentManager::findByPath(const QString& path) const {
    if (path.isEmpty()) return nullptr;
    QFileInfo fi(path);
    const QString abs = fi.absoluteFilePath();
    for (const auto& d : docs_) {
        if (d && d->path() == abs) return d;
    }
    return nullptr;
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