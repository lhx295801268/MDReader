#include "ui/OutlineView.h"
#include <QStandardItemModel>

OutlineView::OutlineView(QWidget* parent) : QTreeView(parent) {
    setHeaderHidden(true);
    setModel(new QStandardItemModel(this));
    connect(this, &QTreeView::clicked, this, [this](const QModelIndex& idx) {
        int line = model()->data(idx, Qt::UserRole + 1).toInt();
        QString slug = model()->data(idx, Qt::UserRole + 2).toString();
        emit headingActivated(line, slug);
    });
}

void OutlineView::setOutline(const QList<OutlineExtractor::Entry>& entries) {
    auto* m = qobject_cast<QStandardItemModel*>(model());
    m->clear();
    QStandardItem* curParent[7] = {nullptr};
    for (const auto& e : entries) {
        auto* item = new QStandardItem(QString("%1 %2").arg(e.level, 1, 10).arg(e.text));
        item->setData(e.lineNumber, Qt::UserRole + 1);
        item->setData(e.slug, Qt::UserRole + 2);
        if (e.level == 1 || !curParent[e.level - 1]) m->appendRow(item);
        else curParent[e.level - 1]->appendRow(item);
        if (e.level < 7) curParent[e.level] = item;
    }
    expandAll();
}
