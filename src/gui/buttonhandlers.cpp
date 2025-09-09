#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "mainwindow.hpp"
#include "refreshfilesystems_helpers.hpp"
#include <filesystem>
#include <QMessageBox>

namespace fs = std::filesystem;

void
MainWindow::handle_start()
{
    if (!komander->do_i_have_root_permissions()) return;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        for (int i = 0; i < fs_table->rowCount(); ++i)
            selected_rows.append(fs_table->model()->index(i, 0));
    }

    QList<QFuture<bool>> futures;
    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString();

        if (!is_running(status)) {
            // Compute current free space
            qint64 free_bytes = QString::fromStdString(
                komander->btrfstat(uuid.toStdString(), "free")
            ).toLongLong();

            // Save starting free space if not already present
            fs::path start_file = fs::path("/tmp") / (".beekeeper-" + uuid.toStdString()) / "startingfreespace";
            fs::create_directories(start_file.parent_path());
            if (!fs::exists(start_file)) {
                std::ofstream ofs(start_file);
                if (ofs.is_open()) {
                    ofs << free_bytes;
                    ofs.close();
                }
            }

            futures.append(komander->async->beesstart(uuid));
        }
    }

    if (futures.isEmpty()) return;

    auto remaining = new int(futures.size());
    auto success_count = new int(0);
    for (auto &f : futures) {
        auto *watcher = new QFutureWatcher<bool>(this);
        watcher->setFuture(f);

        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, remaining, success_count]() {
            if (watcher->result()) (*success_count)++;
            watcher->deleteLater();

            (*remaining)--;
            if (*remaining == 0) {
                int failed_count = *success_count - (*success_count); // compute failures if needed
                QMetaObject::invokeMethod(this->rootThread,
                          [this]() { emit this->rootThread->command_finished("start", "success", ""); },
                          Qt::QueuedConnection);
                delete remaining;
                delete success_count;
            }
        });
    }

    // Show the "started with and now you have" right at start
    refresh_fs_helpers::update_status_manager(fs_table, statusManager);
}

void
MainWindow::handle_stop()
{
    if (!komander->do_i_have_root_permissions()) return;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        for (int i = 0; i < fs_table->rowCount(); ++i)
            selected_rows.append(fs_table->model()->index(i, 0));
    }

    QList<QFuture<bool>> futures;
    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString();
        if (is_running(status)) {
            futures.append(komander->async->beesstop(uuid));
        }
    }

    if (futures.isEmpty()) return;

    auto remaining = new int(futures.size());
    auto success_count = new int(0);
    for (auto &f : futures) {
        auto *watcher = new QFutureWatcher<bool>(this);
        watcher->setFuture(f);

        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, remaining, success_count]() {
            if (watcher->result()) (*success_count)++;
            watcher->deleteLater();

            (*remaining)--;
            if (*remaining == 0) {
                int failed_count = *remaining - *success_count; // or recompute properly
                QMetaObject::invokeMethod(this->rootThread,
                          [this]() { emit this->rootThread->command_finished("stop", "success", ""); },
                          Qt::QueuedConnection);
                delete remaining;
                delete success_count;
            }
        });
    }
}

void
MainWindow::handle_setup()
{
    if (!komander->do_i_have_root_permissions()) return;

    QStringList uuids_to_setup;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        // No selection → consider all rows
        for (int row = 0; row < fs_table->rowCount(); ++row)
            uuids_to_setup.append(fs_table->item(row, 0)->data(Qt::UserRole).toString());
    } else {
        // Only selected rows
        for (auto idx : selected_rows)
            uuids_to_setup.append(fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString());
    }

    if (!uuids_to_setup.isEmpty()) {
        // SetupDialog internally filters only unconfigured UUIDs
        SetupDialog dlg(uuids_to_setup, this);
        dlg.exec();
        refresh_filesystems();
    }
}

// ----------- CONFIG REMOVAL LOGIC ------------

// Called when selection changes; only toggles the toolbar remove button.
void
MainWindow::toggle_remove_button_enabled()
{
    if (!komander->do_i_have_root_permissions()) {
        remove_btn->setEnabled(false);
        return;
    }

    const auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        remove_btn->setEnabled(false);
        return;
    }

    // Check if any selected filesystem is running
    for (auto idx : selected_rows) {
        QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
        DEBUG_LOG("Iterating for toggle remove button. row status: " + status);
        if (status.startsWith("running")) {
            remove_btn->setEnabled(false);
            return;
        }
    }

    // Enable remove button if at least one selected FS is configured
    remove_btn->setEnabled(at_least_one_configured(false));
}

void
MainWindow::handle_remove_button()
{
    if (!komander->do_i_have_root_permissions()) return;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        QMessageBox::information(this, "No selection", "Please select at least one filesystem to remove its configuration.");
        return;
    }

    // Collect selected filesystems that actually have a configuration
    QStringList uuids_to_remove;
    bool blocked_running = false;
    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();

        // Check status to forbid removal of running configs
        QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
        if (status.startsWith("running")) {
            blocked_running = true;
            DEBUG_LOG("Skipping deletion of running filesystem config: " + uuid.toStdString());
            continue;
        }

        std::string path = komander->btrfstat(uuid.toStdString(), "");

        // trim everything after the ':' and remove whitespaces
        path = bk_util::trim_string(bk_util::trim_config_path_after_colon(path));

        if (!path.empty())
            uuids_to_remove.append(uuid);
    }

    if (blocked_running) {
        QMessageBox::warning(
            this,
            "Forbidden",
            "One or more selected filesystems are being deduplicated.\n"
            "Deleting the configuration of a running deduplication daemon is forbidden."
        );
        return;
    }

    if (uuids_to_remove.isEmpty()) {
        QMessageBox::information(this, "No configuration found", "None of the selected filesystems have a configuration to remove.");
        return;
    }

    // Confirm deletion
    QString line1 = tr("You're about to remove the file deduplication engine configuration.\n");
    QString line2 = tr("This does not provoke data loss but you won't have file deduplication functionality unless you set up Beesd again by selecting the filesystem and clicking the Setup button.\n\n");
    QString line3 = tr("Are you sure?");

    QString message = line1 + line2 + line3;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Confirm removal"),
        message,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) return;

    // Remove each config one at a time
    for (const QString &q : uuids_to_remove) {
        std::string uuid = q.toStdString();
        std::string path = komander->btrfstat(uuid, "");

        auto pos = path.find(':');
        if (pos != std::string::npos)
            path = path.substr(pos + 1);
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);

        // delete it
        DEBUG_LOG("Removing config file for uuid ", uuid);
        komander->beesremoveconfig(uuid);
    }

    // Refresh table and statuses
    toggle_remove_button_enabled();
    refresh_filesystems();
}