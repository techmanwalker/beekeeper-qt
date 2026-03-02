#include "mainwindow.hpp"
#include <QToolTip>

bool
MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!fs_table)
        return QMainWindow::eventFilter(obj, event);

    // Handle table hover events
    if (obj == fs_table->viewport() &&
        event->type() == QEvent::MouseMove) {

        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        QModelIndex index = fs_table->indexAt(me->pos());

        if (index.isValid()) {
            // Get UUID from UserRole of column 0
            QTableWidgetItem *uuid_item =
                fs_table->item(index.row(), 0);

            if (!uuid_item)
                return false;

            QString uuid =
                uuid_item->data(Qt::UserRole).toString();

            if (uuid.isEmpty() || uuid == current_hovered_uuid)
                return false;

            current_hovered_uuid = uuid;

        } else {
            // Mouse not over any row
            if (!current_hovered_uuid.isEmpty()) {
                current_hovered_uuid.clear();
            }
        }

        return false; // Let the event propagate
    }

    return QMainWindow::eventFilter(obj, event);
}
