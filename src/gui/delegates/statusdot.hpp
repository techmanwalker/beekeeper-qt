#pragma once

#include <QStyledItemDelegate>

class StatusDotDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* p,
               const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& idx) const override;
};
