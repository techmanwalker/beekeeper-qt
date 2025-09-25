#include "mainwindow.hpp"
#include "../polkit/multicommander.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"
#include "refreshfilesystems_helpers.hpp"

#include <unordered_set>


// ----- Filesystem table builders -----


void MainWindow::refresh_filesystems()
{
    // disable some buttons early if no root (optional)
    update_button_states(); // quick check (must be safe on GUI thread)

    // Ask async btrfsls and hand its future to the builder. Returns immediately.
    auto future_fs = komander->async->btrfsls(); // QFuture<fs_vec>
    build_filesystem_table(future_fs);
}

void
MainWindow::build_filesystem_table(QFuture<fs_vec> filesystem_list_future)
{
    // If a refresh is already running, bail out immediately.
    bool expected = false;
    if (!is_being_refreshed.compare_exchange_strong(expected, true)) {
        DEBUG_LOG("[refresh] already running - skipping new refresh");
        return;
    }

    // Run entire pipeline in a background QtConcurrent thread to return immediately.
    last_build_future = QtConcurrent::run([this, filesystem_list_future]() mutable {
        // 1) Wait the filesystem_list_future to be ready (it was created by komander->async->btrfsls())
        filesystem_list_future.waitForFinished();
        fs_vec filesystem_list = filesystem_list_future.result(); // local copy

        // 2) Read the current table snapshot - must be done on GUI thread
        fs_vec table_filesystems;
        QMetaObject::invokeMethod(this, [this, &table_filesystems]() {
            // Build table snapshot
            for (int r = 0; r < fs_table->rowCount(); ++r) {
                fs_map map;
                auto *uuid_item = fs_table->item(r, 0);
                auto *label_item = fs_table->item(r, 1);
                auto *status_item = fs_table->item(r, 2);
                if (uuid_item) map["uuid"] = uuid_item->data(Qt::UserRole).toString().toStdString();
                if (label_item) map["label"] = label_item->data(Qt::UserRole).toString().toStdString();
                if (status_item) map["status"] = bk_util::trim_string(status_item->data(Qt::UserRole).toString().toStdString());
                if (!map["uuid"].empty()) table_filesystems.push_back(std::move(map));
            }
        }, Qt::BlockingQueuedConnection); // blocking so background thread can continue with the snapshot

        // 3) Compute new / removed / existing using the utility functions
        fs_vec *new_filesystems = new fs_vec(bk_util::subtract_vectors_of_maps(filesystem_list, table_filesystems, std::string("uuid")));
        fs_vec *removed_filesystems = new fs_vec(bk_util::subtract_vectors_of_maps(table_filesystems, filesystem_list, std::string("uuid")));
        fs_vec *existing_filesystems = new fs_vec(bk_util::subtract_vectors_of_maps(table_filesystems, *removed_filesystems, std::string("uuid")));

        // 4) Dispatch the three tasks in parallel (each returns QFuture<void>)
        QFuture<void> f_add = add_new_rows_task(new_filesystems);
        QFuture<void> f_remove = remove_old_rows_task(removed_filesystems);
        QFuture<void> f_update = update_existing_rows_task(existing_filesystems);

        // 5) Wait for them to finish here in background (no UI blocking)
        f_add.waitForFinished();
        f_remove.waitForFinished();
        f_update.waitForFinished();

        // 6) Now update button states on GUI thread
        QMetaObject::invokeMethod(this, [this]() {
            update_button_states();
            refresh_fs_helpers::update_status_manager(
                fs_table,
                statusManager
            );
        }, Qt::QueuedConnection);

        // 7) Release the guard
        is_being_refreshed.store(false);

    }); // QtConcurrent::run
}

QFuture<void> MainWindow::add_new_rows_task(fs_vec *new_filesystems)
{
    return QtConcurrent::run([this, new_filesystems]() {
        if (!new_filesystems || new_filesystems->empty()) {
            delete new_filesystems;
            return;
        }

        // Build a lightweight list of rows to add (do NOT construct QTableWidgetItems here).
        struct RowToAdd { std::string uuid, label, status; };
        std::vector<RowToAdd> rows;
        rows.reserve(new_filesystems->size());
        for (const auto &m : *new_filesystems) {
            RowToAdd r;
            auto it = m.find("uuid"); if (it != m.end()) r.uuid = it->second;
            it = m.find("label"); if (it != m.end()) r.label = it->second;
            it = m.find("status"); if (it != m.end()) r.status = it->second;
            if (!r.uuid.empty())
                rows.push_back(std::move(r));
        }

        // Now schedule a GUI-thread batch insertion
        QMetaObject::invokeMethod(this, [this, rows = std::move(rows)]() mutable {
            fs_table->setUpdatesEnabled(false);
            // Optionally disable sorting while we insert:
            bool hadSorting = fs_table->isSortingEnabled();
            fs_table->setSortingEnabled(false);

            for (const auto &r : rows) {
                int row = fs_table->rowCount();
                fs_table->insertRow(row);

                auto *uuid_item = new QTableWidgetItem(QString::fromStdString(r.uuid));
                uuid_item->setData(Qt::UserRole, QString::fromStdString(r.uuid));
                uuid_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                fs_table->setItem(row, 0, uuid_item);

                auto *label_item = new QTableWidgetItem(QString::fromStdString(r.label));
                label_item->setData(Qt::UserRole, QString::fromStdString(r.label));
                label_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                fs_table->setItem(row, 1, label_item);

                refresh_fs_helpers::status_text_mapper mapper;
                QString display_status = mapper.map_status_text(QString::fromStdString(r.status));
                auto *status_item = new QTableWidgetItem(display_status);
                status_item->setData(Qt::UserRole, QString::fromStdString(r.status));
                status_item->setData(Qt::DisplayRole, display_status);
                status_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                fs_table->setItem(row, 2, status_item);
            }

            // restore sorting and updates
            fs_table->setSortingEnabled(hadSorting);
            fs_table->setUpdatesEnabled(true);
        }, Qt::QueuedConnection);

        delete new_filesystems;
    });
}

QFuture<void> MainWindow::remove_old_rows_task(fs_vec *removed_filesystems)
{
    return QtConcurrent::run([this, removed_filesystems]() {
        if (!removed_filesystems || removed_filesystems->empty()) {
            delete removed_filesystems;
            return;
        }

        // Build set of UUIDs to remove
        std::unordered_set<std::string> uuids;
        uuids.reserve(removed_filesystems->size()*2);
        for (const auto &m : *removed_filesystems) {
            auto it = m.find("uuid");
            if (it != m.end()) uuids.insert(it->second);
        }

        // Schedule GUI removal batch
        QMetaObject::invokeMethod(this, [this, uuids = std::move(uuids)]() mutable {
            fs_table->setUpdatesEnabled(false);
            bool hadSorting = fs_table->isSortingEnabled();
            fs_table->setSortingEnabled(false);

            // iterate rows backwards to safely remove rows
            for (int r = fs_table->rowCount() - 1; r >= 0; --r) {
                auto *uuid_item = fs_table->item(r, 0);
                if (!uuid_item) continue;
                std::string u = uuid_item->data(Qt::UserRole).toString().toStdString();
                if (uuids.find(u) != uuids.end()) {
                    fs_table->removeRow(r);
                }
            }

            fs_table->setSortingEnabled(hadSorting);
            fs_table->setUpdatesEnabled(true);
        }, Qt::QueuedConnection);

        delete removed_filesystems;
    });
}

QFuture<void> MainWindow::update_existing_rows_task(fs_vec *existing_filesystems)
{
    return QtConcurrent::run([this, existing_filesystems]() {
        if (!existing_filesystems || existing_filesystems->empty()) {
            delete existing_filesystems;
            return;
        }

        // Build map uuid -> map of new fields for quick lookup
        std::unordered_map<std::string, fs_map> update_map;
        update_map.reserve(existing_filesystems->size()*2);
        
        // Re-fetch status for each existing filesystem
        for (const auto &m : *existing_filesystems) {
            auto it = m.find("uuid");
            if (it != m.end()) {
                fs_map updated = m;
                // Re-query the actual status right now
                std::string fresh_status = komander->beesstatus(it->second);
                updated["status"] = fresh_status;
                update_map.emplace(it->second, updated);
            }
        }

        // Schedule GUI update batch
        QMetaObject::invokeMethod(this, [this, update_map = std::move(update_map)]() mutable {
            fs_table->setUpdatesEnabled(false);
            bool hadSorting = fs_table->isSortingEnabled();
            fs_table->setSortingEnabled(false);

            for (int r = 0; r < fs_table->rowCount(); ++r) {
                auto *uuid_item = fs_table->item(r, 0);
                if (!uuid_item) continue;
                std::string u = uuid_item->data(Qt::UserRole).toString().toStdString();
                auto it = update_map.find(u);
                if (it == update_map.end()) continue;

                const fs_map &m = it->second;
                // Update label
                auto itlab = m.find("label");
                if (itlab != m.end()) {
                    auto *label_item = fs_table->item(r, 1);
                    if (label_item) {
                        label_item->setData(Qt::UserRole, QString::fromStdString(itlab->second));
                        label_item->setText(QString::fromStdString(itlab->second));
                    }
                }
                // Update status
                auto itst = m.find("status");
                if (itst != m.end()) {
                    auto *status_item = fs_table->item(r, 2);
                    if (status_item) {
                        QString raw_status = QString::fromStdString(itst->second);
                        status_item->setData(Qt::UserRole, raw_status);
                        
                        // Map the status for display
                        refresh_fs_helpers::status_text_mapper mapper;
                        QString display_status = mapper.map_status_text(raw_status);
                        status_item->setData(Qt::DisplayRole, display_status);
                        status_item->setText(display_status);
                    }
                }
            }

            fs_table->setSortingEnabled(hadSorting);
            fs_table->setUpdatesEnabled(true);

        }, Qt::QueuedConnection);

        delete existing_filesystems;
    });
}
