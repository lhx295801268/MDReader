#include "ui/OutlineView.h"
#include <QStandardItemModel>

namespace {
constexpr int kMinLevel = 1;        // smallest Markdown heading level
constexpr int kMaxLevel = 6;        // largest Markdown heading level
constexpr int kParentSlots = kMaxLevel + 1;  // index 0 sentinel + 1..6 per level
constexpr int kLineRole = Qt::UserRole + 1;
constexpr int kSlugRole = Qt::UserRole + 2;
}  // namespace

OutlineView::OutlineView(QWidget* parent) : QTreeView(parent) {
    setHeaderHidden(true);
    setModel(new QStandardItemModel(this));
    connect(this, &QTreeView::clicked, this, [this](const QModelIndex& idx) {
        int line = model()->data(idx, kLineRole).toInt();
        QString slug = model()->data(idx, kSlugRole).toString();
        emit headingActivated(line, slug);
    });
}

void OutlineView::setOutline(const QList<OutlineExtractor::Entry>& entries) {
    // Markdown headings are level 1..6. curParent[0] is unused (sentinel),
    // curParent[1..6] track the most recent item at each level so a heading
    // at level N can be nested under curParent[N-1]. If the parent at level N-1
    // is missing (e.g., a level-3 heading preceded by a level-1), we fall back
    // to top-level via the (level == 1 || !curParent[level-1]) guard.
    auto* m = qobject_cast<QStandardItemModel*>(model());
    m->clear();
    QStandardItem* curParent[kParentSlots] = {nullptr};
    for (const auto& e : entries) {
        auto* item = new QStandardItem(QStringLiteral("%1 %2").arg(e.level).arg(e.text));
        item->setData(e.lineNumber, kLineRole);
        item->setData(e.slug, kSlugRole);
        if (e.level == 1 || !curParent[e.level - 1]) m->appendRow(item);
        else curParent[e.level - 1]->appendRow(item);
        if (e.level < 7) curParent[e.level] = item;
    }
    expandAll();
}