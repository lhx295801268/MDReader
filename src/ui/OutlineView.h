#pragma once
#include <QTreeView>
#include "services/OutlineExtractor.h"

class OutlineView : public QTreeView {
    Q_OBJECT
public:
    explicit OutlineView(QWidget* parent = nullptr);
    /// Rebuilds the tree model from entries. Always reflects the latest setOutline call.
    void setOutline(const QList<OutlineExtractor::Entry>& entries);

signals:
    void headingActivated(int lineNumber, const QString& slug);
};
