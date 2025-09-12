#include "mainwindow.hpp"

/**
 * @brief Adds the selected configured filesystems to the autostart list.
 *
 * This function reads the currently selected rows in fs_table and adds
 * the corresponding UUIDs to /etc/bees/beekeeper-qt.cfg if they are
 * not already present. Only configured filesystems are considered.
 *
 * The autostart file is read by the systemd-activated beekeeper-helper
 * service at boot, which starts Beesd for each UUID listed here.
 */
void
MainWindow::handle_add_to_autostart()
{
    QModelIndexList selected = fs_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    auto futures = new QList<QFuture<bool>>; // heap allocation

    for (const QModelIndex &idx : selected) {
        if (!is_configured(idx))
            continue;

        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        if (bk_util::is_uuid_in_autostart(uuid.toStdString()))
            continue;

        futures->append(komander->async->add_uuid_to_autostart(uuid));
    }

    process_fs_async(futures);
}

/**
 * @brief Removes the selected configured filesystems from the autostart list.
 *
 * This function reads /etc/bees/beekeeper-qt.cfg, removes the UUIDs
 * corresponding to the selected configured rows in fs_table, and
 * rewrites the file without empty lines.
 *
 * Only configured filesystems are considered.
 */
void
MainWindow::handle_remove_from_autostart()
{
    QModelIndexList selected = fs_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    auto futures = new QList<QFuture<bool>>; // heap allocation

    for (const QModelIndex &idx : selected) {
        if (!is_configured(idx))
            continue;

        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        if (!bk_util::is_uuid_in_autostart(uuid.toStdString()))
            continue;

        futures->append(komander->async->remove_uuid_from_autostart(uuid));
    }

    if (!futures->isEmpty())
        process_fs_async(futures);
}