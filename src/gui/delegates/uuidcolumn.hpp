#pragma once
#include <QStyledItemDelegate>

class UUIDColumnDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& idx) const override;
};
