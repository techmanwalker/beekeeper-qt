#include "mainwindow.hpp"
#include "refreshfilesystems_helpers.hpp"
#include <QToolTip>

void
MainWindow::update_status_bar()
{
    if (!fs_table || !statusBar) return; // <-- avoids crash at startup

    int selected_count = refresh_fs_helpers::selected_rows_count(fs_table);

    // New: peek hovered UUID even when multiple selections
    if (selected_count > 1 && !current_hovered_uuid.isEmpty()) {
        QString message = tr("Multiple filesystems selected. To show free space statistics, select only one.");
        QString uuid;

        statusBar->showMessage(message);
        if (!uuid.isEmpty()) {
            emit status_updated(uuid, message);
        }
    } else {
        // Fallback: nothing selected, no hover
        statusBar->clearMessage();
        emit status_updated(QString(), QString());
    }

    refresh_fs_helpers::update_status_manager(
        fs_table,
        statusManager
    );
}

bool
MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!fs_table) return QMainWindow::eventFilter(obj, event);

    // Handle table hover events
    if (obj == fs_table->viewport() && event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QPoint pos = me->pos();
        QModelIndex index = fs_table->indexAt(pos);
        
        if (index.isValid()) {
            // Get UUID from UserRole of column 0
            QTableWidgetItem *uuid_item = fs_table->item(index.row(), 0);
            if (uuid_item) {
                QString uuid = uuid_item->data(Qt::UserRole).toString();
                if (!uuid.isEmpty() && uuid != current_hovered_uuid) {
                    current_hovered_uuid = uuid;
                    
                    // Update status for this specific UUID
                    refresh_fs_helpers::update_status_manager_one_uuid(
                        fs_table,
                        statusManager,
                        current_hovered_uuid
                    );
                    
                    // Get and display the status
                    QString status = statusManager.get_status(uuid);
                    if (!status.isEmpty()) {
                        statusBar->showMessage(status);
                    }
                }
            }
        } else {
            // Mouse not over any row
            if (!current_hovered_uuid.isEmpty()) {
                current_hovered_uuid.clear();
                update_status_bar(); // This will clear or show selection status
            }
        }
        
        return false; // Let the event propagate
    }
    
    return QMainWindow::eventFilter(obj, event);
}