#include "mainwindow.hpp"
#include "refreshfilesystems_helpers.hpp"
#include <QToolTip>

void
MainWindow::update_status_bar()
{
    if (!fs_table || !statusBar) return; // <-- avoids crash at startup

    int selected_count = selected_rows_count();

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

    refresh_fs_helpers::update_status_manager(fs_table, statusManager);
}

// Change the status bar to show the current fs freed space when hovered
bool
MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!fs_table || !statusBar) return false;

    QString message;
    int selected_count = selected_rows_count();

    // -------------------------------------------------
    // Table hover handling
    // -------------------------------------------------
    if (obj == fs_table->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            QModelIndex idx = fs_table->indexAt(me->pos());
            QString hovered;
            if (idx.isValid() && fs_table->item(idx.row(), 0))
                hovered = idx.sibling(idx.row(), 0).data(Qt::UserRole).toString();
            set_hovered_uuid(hovered);

            message = statusManager.get_status(hovered);
            if (!message.isEmpty())
                statusBar->showMessage(message);

            update_status_bar(); // still responsible for multiple selection warning
        } else if (event->type() == QEvent::Leave) {
            set_hovered_uuid(QString());

            // Clear message only if no selection and no hover
            // if (selected_count <= 0)
                // statusBar->clearMessage();

            update_status_bar();
        }
    }

    // -------------------------------------------------
    // StatusBar hover handling
    // -------------------------------------------------
    else if (obj == statusBar) {
        // Only show tooltip if the statusBar is already created and there is a message
        if ((event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) &&
            (!current_hovered_uuid.isEmpty() || !selected_configured_filesystems().isEmpty()))
        {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QToolTip::showText(me->globalPosition().toPoint(), statusBar->currentMessage(), statusBar);
        } else if (event->type() == QEvent::Leave) {
            QToolTip::hideText();
        }
    }

    if (!message.isEmpty())
        statusBar->showMessage(message);

    return QMainWindow::eventFilter(obj, event);
}