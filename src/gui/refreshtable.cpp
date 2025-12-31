#include "mainwindow.hpp"
#include "../polkit/globals.hpp"
#include "../polkit/multicommander.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"
#include "refreshfilesystems_helpers.hpp"

#include <unordered_set>
#include <unordered_map>


// ----- Filesystem table builders -----


void MainWindow::refresh_filesystems()
{
    update_button_states();
    auto future_fs = komander->async->btrfsls();
    build_filesystem_table(future_fs);
}

void MainWindow::build_filesystem_table(QFuture<fs_vec> filesystem_list_future)
{
    // Avoid simultaneous refreshes
    bool expected = false;
    if (!is_being_refreshed.compare_exchange_strong(expected, true)) {
        DEBUG_LOG("[refresh] already running - skipping new refresh");
        return;
    }

    last_build_future = QtConcurrent::run([this, filesystem_list_future]() mutable {
        // Temporarily disable sorting
        bool hadSorting = fs_table->isSortingEnabled();
        fs_table->setSortingEnabled(false);
        
        // 1) Wait for the filesystem list
        filesystem_list_future.waitForFinished();
        fs_vec filesystem_list = filesystem_list_future.result();

        // 2) Convert filesystem_list to an UUID-indexed map
        //    This is our ONLY SOURCE OF TRUTH for our entire pipeline
        std::unordered_map<std::string, fs_map> fs_data_by_uuid;
        for (const auto &fs : filesystem_list) {
            auto it = fs.find("uuid");
            if (it != fs.end() && !it->second.empty()) {
                fs_data_by_uuid[it->second] = fs;
            }
        }

        // 3) Read current table snapshot (only UUIDs)
        std::unordered_set<std::string> current_uuids;
        QMetaObject::invokeMethod(this, [this, &current_uuids]() {
            for (int r = 0; r < fs_table->rowCount(); ++r) {
                auto *uuid_item = fs_table->item(r, 0);
                if (uuid_item) {
                    std::string uuid = uuid_item->data(Qt::UserRole).toString().toStdString();
                    if (!uuid.empty()) {
                        current_uuids.insert(uuid);
                    }
                }
            }
        }, Qt::BlockingQueuedConnection);

        // 4) Determine what to add and remove based on fs_data_by_uuid
        std::vector<std::string> to_add;
        for (const auto &pair : fs_data_by_uuid) {
            const std::string &uuid = pair.first;
            if (current_uuids.find(uuid) == current_uuids.end()) {
                to_add.push_back(uuid);
            }
        }

        std::vector<std::string> to_remove;
        for (const auto &uuid : current_uuids) {
            if (fs_data_by_uuid.find(uuid) == fs_data_by_uuid.end()) {
                to_remove.push_back(uuid);
            }
        }

        // ========== CYCLE 1: ADD AND REMOVE ==========
        QFuture<void> f_add = add_new_rows_task(to_add);
        QFuture<void> f_remove = remove_old_rows_task(to_remove);

        // Wait for both operations to finish
        f_add.waitForFinished();
        f_remove.waitForFinished();

        // ========== CYCLE 2: UPDATE EVERYTHING ==========
        // Now that the indices are stable, we'll update the entire table
        // using ONLY the data from fs_data_by_uuid
        update_all_rows_task(fs_data_by_uuid);

        // 5) Update buttons and status_manager
        QMetaObject::invokeMethod(this, [this]() {
            update_button_states();
            refresh_fs_helpers::update_status_manager(fs_table, statusManager);
        }, Qt::QueuedConnection);

        // 6) Release guard and restore sorting
        is_being_refreshed.store(false);
        fs_table->setSortingEnabled(hadSorting);
    });
}

QFuture<void>
MainWindow::add_new_rows_task(const std::vector<std::string> &uuids_to_add)
{
    return QtConcurrent::run([this, uuids_to_add]() {
        if (uuids_to_add.empty()) {
            return;
        }

        // Añadir filas en la GUI (solo estructura básica, sin datos)
        QMetaObject::invokeMethod(this, [this, uuids_to_add]() {
            fs_table->setUpdatesEnabled(false);
            bool hadSorting = fs_table->isSortingEnabled();
            fs_table->setSortingEnabled(false);

            for (const auto &uuid : uuids_to_add) {
                // Verificar por si acaso que no exista ya
                bool exists = false;
                for (int r = 0; r < fs_table->rowCount(); ++r) {
                    auto *uuid_item = fs_table->item(r, 0);
                    if (uuid_item && 
                        uuid_item->data(Qt::UserRole).toString().toStdString() == uuid) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    int row = fs_table->rowCount();
                    fs_table->insertRow(row);

                    // UUID (solo el identificador, sin datos)
                    auto *uuid_item = new QTableWidgetItem(QString::fromStdString(uuid));
                    uuid_item->setData(Qt::UserRole, QString::fromStdString(uuid));
                    uuid_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                    fs_table->setItem(row, 0, uuid_item);

                    // Label vacío (se actualizará después en update_all_rows_task)
                    auto *label_item = new QTableWidgetItem("");
                    label_item->setData(Qt::UserRole, "");
                    label_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                    fs_table->setItem(row, 1, label_item);

                    // Status vacío (se actualizará después en update_all_rows_task)
                    auto *status_item = new QTableWidgetItem("");
                    status_item->setData(Qt::UserRole, "");
                    status_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                    fs_table->setItem(row, 2, status_item);
                }
            }

            fs_table->setSortingEnabled(hadSorting);
            fs_table->setUpdatesEnabled(true);
        }, Qt::QueuedConnection);
    });
}

QFuture<void>
MainWindow::remove_old_rows_task(const std::vector<std::string> &uuids_to_remove)
{
    return QtConcurrent::run([this, uuids_to_remove]() {
        if (uuids_to_remove.empty()) {
            return;
        }

        // Convertir a set para búsqueda rápida
        std::unordered_set<std::string> remove_set(
            uuids_to_remove.begin(), 
            uuids_to_remove.end()
        );

        // Quitar filas de la GUI
        QMetaObject::invokeMethod(this, [this, remove_set = std::move(remove_set)]() {
            fs_table->setUpdatesEnabled(false);
            bool hadSorting = fs_table->isSortingEnabled();
            fs_table->setSortingEnabled(false);

            // Iterar de atrás hacia adelante para evitar problemas con índices
            for (int r = fs_table->rowCount() - 1; r >= 0; --r) {
                auto *uuid_item = fs_table->item(r, 0);
                if (uuid_item) {
                    std::string uuid = uuid_item->data(Qt::UserRole).toString().toStdString();
                    if (remove_set.find(uuid) != remove_set.end()) {
                        fs_table->removeRow(r);
                    }
                }
            }

            fs_table->setSortingEnabled(hadSorting);
            fs_table->setUpdatesEnabled(true);
        }, Qt::QueuedConnection);
    });
}

void
MainWindow::update_all_rows_task(
    const std::unordered_map<std::string, fs_map> &fs_data_by_uuid
)
{
    // Esta función se ejecuta en el mismo hilo background que build_filesystem_table
    // NO hace consultas adicionales, SOLO usa fs_data_by_uuid como fuente de verdad
    
    // Actualizar la tabla con los datos de fs_data_by_uuid
    QMetaObject::invokeMethod(this, [this, fs_data_by_uuid]() {
        fs_table->setUpdatesEnabled(false);
        bool hadSorting = fs_table->isSortingEnabled();
        fs_table->setSortingEnabled(false);

        for (int r = 0; r < fs_table->rowCount(); ++r) {
            auto *uuid_item = fs_table->item(r, 0);
            if (!uuid_item) continue;

            std::string uuid = uuid_item->data(Qt::UserRole).toString().toStdString();
            auto it = fs_data_by_uuid.find(uuid);
            if (it == fs_data_by_uuid.end()) continue;

            const fs_map &fs_data = it->second;

            // REGLA 1: column 0 (UUID) ya está correcta desde add_new_rows_task
            // Solo verificamos que Qt::UserRole esté bien (debería estarlo)
            auto uuid_it = fs_data.find("uuid");
            if (uuid_it != fs_data.end()) {
                uuid_item->setData(Qt::UserRole, QString::fromStdString(uuid_it->second));
                uuid_item->setText(QString::fromStdString(uuid_it->second));
            }

            // REGLA 2: column 1 (Label) = filesystem_list[uuid]["label"]
            auto label_it = fs_data.find("label");
            if (label_it != fs_data.end()) {
                auto *label_item = fs_table->item(r, 1);
                if (label_item) {
                    QString label = QString::fromStdString(label_it->second);
                    label_item->setData(Qt::UserRole, label);
                    label_item->setText(label);
                }
            }

            // REGLA 3: column 2 (Status) = filesystem_list[uuid]["status"]
            auto status_it = fs_data.find("status");
            if (status_it != fs_data.end()) {
                auto *status_item = fs_table->item(r, 2);
                if (status_item) {
                    QString raw_status = QString::fromStdString(status_it->second);
                    status_item->setData(Qt::UserRole, raw_status);
                    
                    // Mapear el status para display
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
}