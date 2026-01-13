#include "beekeeper/beesdmgmt.hpp"
#include "mainwindow.hpp"
#include "tablecheckers.hpp"
#include "../polkit/globals.hpp"

using namespace tablecheckers;

/**
 * @brief Adds the selected configured filesystems to the autostart list.
 *
 * This function reads the currently selected rows in fs_table and adds
 * the corresponding UUIDs to /etc/bees/autostartsettings.cfg if they are
 * not already present. Only configured filesystems are considered.
 *
 * The autostart file is read by the systemd-activated thebeekeeper
 * service at boot, which starts Beesd for each UUID listed here.
 */
void
MainWindow::handle_add_to_autostart()
{
    QModelIndexList selected = list_of_selected_rows(fs_table, false);

    auto futures = new QList<QFuture<bool>>; // heap allocation

    for (const QModelIndex &idx : selected) {
        if (!configured(idx, fs_view_state))
            continue;

        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        if (bk_mgmt::autostart::is_enabled_for(uuid.toStdString()))
            continue;

        // be optimistic
        fs_view_state[
            uuid.toStdString()
        ].autostart = true;

        futures->append(komander->add_uuid_to_autostart(uuid));
    }

    refresh_after_these_futures_finish(futures);
}

/**
 * @brief Removes the selected configured filesystems from the autostart list.
 *
 * This function reads /etc/bees/autostartsettings.cfg, removes the UUIDs
 * corresponding to the selected configured rows in fs_table, and
 * rewrites the file without empty lines.
 *
 * Only configured filesystems are considered.
 */
void
MainWindow::handle_remove_from_autostart()
{
    QModelIndexList selected = list_of_selected_rows(fs_table, false);

    auto futures = new QList<QFuture<bool>>; // heap allocation

    for (const QModelIndex &idx : selected) {
        if (!configured(idx, fs_view_state))
            continue;

        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        if (!bk_mgmt::autostart::is_enabled_for(uuid.toStdString()))
            continue;

        // be optimistic
        fs_view_state[
            uuid.toStdString()
        ].autostart = false;

        futures->append(komander->remove_uuid_from_autostart(uuid));
    }

    if (!futures->isEmpty())
        refresh_after_these_futures_finish(futures);
}