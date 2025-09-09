#include "uuidcolumndelegate.hpp"
#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QMouseEvent>
#include <QEvent>
#include <QString>

void
UUIDColumnDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index); // important: populate opt with index data

    // Draw the background correctly for selected / hovered / keyboard-focus states
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);

    // Draw the centered icon
    QRect r = opt.rect;
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    icon.paint(painter, r, Qt::AlignCenter);

    painter->restore();
}

QSize
UUIDColumnDelegate::sizeHint(const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    int iconSize = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, nullptr);
    int padding = 4; // extra space
    return QSize(iconSize + padding, iconSize + padding);
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
                                   tr("UUID copied to clipboard."));
                return true;
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}