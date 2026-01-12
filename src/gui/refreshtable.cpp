#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "mainwindow.hpp"
#include "../polkit/globals.hpp"
#include "refreshfilesystems_helpers.hpp"
#include <qabstractitemmodel.h>

// ----- Filesystem table builders -----

void
MainWindow::refresh_table(const bool fetch_data_from_daemon)
{
    // ------------------------------------------------------------------
    // Guard against multiple concurrent refreshes
    // ------------------------------------------------------------------
    if (is_being_refreshed.exchange(true)) {
        DEBUG_LOG("Was already refreshing. Discarding...");
    }

    // Immediate UI feedback
    update_button_states();

    // ------------------------------------------------------------------
    // Record what the table is currently rendering
    // This is our "what the user is seeing right now" baseline
    // ------------------------------------------------------------------
    std::unordered_map<std::string, std::string> the_status_table_is_showing_for_uuid;

    if (fs_table) {
        const int row_count = fs_table->rowCount();
        the_status_table_is_showing_for_uuid.reserve(row_count);

        for (int row = 0; row < row_count; ++row) {
            QModelIndex uuid_idx   = fs_table->model()->index(row, 0);
            QModelIndex status_idx = fs_table->model()->index(row, 2);

            QString uuid_qstr   = refresh_fs_helpers::fetch_user_role(uuid_idx, 0);
            QString status_qstr = refresh_fs_helpers::fetch_user_role(status_idx, 2);

            if (!uuid_qstr.isEmpty()) {
                the_status_table_is_showing_for_uuid.emplace(
                    uuid_qstr.toStdString(),
                    status_qstr.toStdString()
                );
            }
        }
    }

    // ------------------------------------------------------------------
    // Heavy work outside the GUI thread
    // ------------------------------------------------------------------
    (void) QtConcurrent::run(
        [this, fetch_data_from_daemon, the_status_table_is_showing_for_uuid]() mutable {

        fs_map fresh_data;

        // ------------------------------------------------------------------
        // Decide where "fresh_data" comes from
        // ------------------------------------------------------------------
        if (fetch_data_from_daemon) {
            // Full refresh: ask the daemon
            fresh_data = komander->btrfsls();

            // Now the GUI's optimistic state is reset to reality
            fs_view_state = fresh_data;
        } else {
            // Soft refresh: trust what the GUI already believes
            fresh_data = fs_view_state;
        }

        // ------------------------------------------------------------------
        // Compute what changed, using what the table was showing as baseline
        // ------------------------------------------------------------------
        fs_diff changes;

        // 1) Changed or removed entries
        for (const auto &[uuid, rendered_status] : the_status_table_is_showing_for_uuid) {

            auto it = fresh_data.find(uuid);

            if (it != fresh_data.end()) {
                // Still exists → check if status changed
                if (rendered_status != it->second.status) {
                    changes.just_changed.emplace(uuid, it->second);
                }
            } else {
                // Not present anymore → only allowed on full refresh
                if (fetch_data_from_daemon) {
                    changes.just_removed.emplace_back(uuid);
                }
            }
        }

        // 2) Newly added entries (only on full refresh)
        if (fetch_data_from_daemon) {
            for (const auto &[uuid, info] : fresh_data) {
                if (the_status_table_is_showing_for_uuid.find(uuid)
                    == the_status_table_is_showing_for_uuid.end()) {
                    changes.newly_added.emplace(uuid, info);
                }
            }
        }

        // ------------------------------------------------------------------
        // Now go back to the GUI thread and apply the dumb renders
        // ------------------------------------------------------------------
        QMetaObject::invokeMethod(
            this,
            [this, changes]() mutable {

                refresh_fs_helpers::status_text_mapper mapper;

                if (fs_table && fs_table->selectionModel()) {
                    fs_table->selectionModel()->blockSignals(true);
                }

                apply_removed(changes.just_removed);
                apply_added(changes.newly_added, mapper);
                apply_changed(changes.just_changed, mapper);

                if (fs_table && fs_table->selectionModel()) {
                    fs_table->selectionModel()->blockSignals(false);
                }

                is_being_refreshed.store(false);

                update_button_states();
            },
            Qt::QueuedConnection
        );
    });
}

/**
* @brief Return the uuids of all the filesystems the table is showing..
* @note This is just an indicator of what the user is SEEING
* right now. This is just meant to control what the table
* is rendering and optimize the refresh.
*/
std::vector<std::string>
MainWindow::list_currently_displayed_filesystems()
{
    std::vector<std::string> displayed_filesystems;
    displayed_filesystems.reserve(fs_table->rowCount());
    
    for (int r = 0; r < fs_table->rowCount(); ++r) {
        auto *uuid_item = fs_table->item(r, 0);
        
        std::string uuid = uuid_item ? uuid_item->data(Qt::UserRole).toString().toStdString() : "";
        
        // Only add if we have a UUID
        if (!uuid.empty()) {
            displayed_filesystems.emplace_back(uuid);
        }
    }
    
    return displayed_filesystems;
}

// ----- REAL DUMB RENDER WORKERS -----

// Add whatever rows "added" tells it to do
void
MainWindow::apply_added(const fs_map &added, refresh_fs_helpers::status_text_mapper &mapper)
{
    for (const auto &[uuid, info] : added) {
        int row = fs_table->rowCount();
        fs_table->insertRow(row);

        auto *uuid_item = new QTableWidgetItem;
        uuid_item->setData(Qt::UserRole, QString::fromStdString(uuid));
        uuid_item->setText(uuid_item->data(Qt::UserRole).toString());

        auto *label_item = new QTableWidgetItem;
        label_item->setData(
            Qt::UserRole,
            QString::fromStdString(info.label)
        );
        label_item->setText(label_item->data(Qt::UserRole).toString());

        auto *status_item = new QTableWidgetItem;
        status_item->setData(
            Qt::UserRole,
            QString::fromStdString(info.status)
        );
        status_item->setText(
            mapper.map_status_text(status_item->data(Qt::UserRole).toString())
        );

        fs_table->setItem(row, 0, uuid_item);
        fs_table->setItem(row, 1, label_item);
        fs_table->setItem(row, 2, status_item);
    }
}

// Remove whatever rows "removed" tells it to do
void
MainWindow::apply_removed(const std::vector<std::string> &removed)
{
    for (const auto &uuid: removed) {
        for (int r = 0; r < fs_table->rowCount(); ++r) {
            auto *item = fs_table->item(r, 0);
            if (!item)
                continue;

            if (item->data(Qt::UserRole).toString().toStdString() == uuid) {
                fs_table->removeRow(r);
                break;
            }
        }
    }
}

// Replace the data of whatever rows "changed" tells it to do
void
MainWindow::apply_changed(const fs_map &changed, refresh_fs_helpers::status_text_mapper &mapper)
{
    for (const auto &[uuid, info] : changed) {
        for (int r = 0; r < fs_table->rowCount(); ++r) {
            auto *uuid_item = fs_table->item(r, 0);
            if (!uuid_item)
                continue;

            if (uuid_item->data(Qt::UserRole).toString().toStdString() == uuid) {

                auto *label_item = fs_table->item(r, 1);
                auto *status_item = fs_table->item(r, 2);

                if (label_item) {
                    label_item->setData(
                        Qt::UserRole,
                        QString::fromStdString(info.label)
                    );
                    label_item->setText(
                        label_item->data(Qt::UserRole).toString()
                    );
                }

                if (status_item) {
                    status_item->setData(
                        Qt::UserRole,
                        QString::fromStdString(info.status)
                    );
                    status_item->setText(
                        mapper.map_status_text(status_item->data(Qt::UserRole).toString())
                    );
                }

                break;
            }
        }
    }
}
