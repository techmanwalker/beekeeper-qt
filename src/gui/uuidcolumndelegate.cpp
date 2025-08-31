#include "uuidcolumndelegate.hpp"
#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QMouseEvent>
#include <QEvent>
#include <QString>

void
UUIDColumnDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt,
                               const QModelIndex& idx) const
{
    Q_UNUSED(idx);
    p->save();

    QRect r = opt.rect;
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    icon.paint(p, r, Qt::AlignCenter);

    p->restore();
}

QSize
UUIDColumnDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    // Use the standard small icon size
    int iconSize = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, nullptr);

    int padding = 4; // optional extra padding
    int side = iconSize + padding;

    return QSize(side, side);
}

bool
UUIDColumnDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    QString uuid = index.data(Qt::UserRole).toString();

    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* me = dynamic_cast<QMouseEvent*>(event)) {
            if (me->button() == Qt::LeftButton) {
                QApplication::clipboard()->setText(uuid);
                QToolTip::showText(me->globalPosition().toPoint(),
                                   // "UUID copied: " + uuid.left(8) + "…"
                                   "UUID copied to clipboard."
                );
                return true;
            }
        }
    } else if (event->type() == QEvent::ToolTip) {
        if (auto* he = dynamic_cast<QHelpEvent*>(event)) {
            // QHelpEvent still uses globalPos() in Qt6
            QToolTip::showText(he->globalPos(), uuid);
            return true;
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
