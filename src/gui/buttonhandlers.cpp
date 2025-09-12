#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "beekeeper/qt-debug.hpp"
#include "mainwindow.hpp"
#include "refreshfilesystems_helpers.hpp"
#include <filesystem>
#include <QMessageBox>
#include <QPushButton>
#include <string>

namespace fs = std::filesystem;

// -------------------------
void
MainWindow::update_button_states()
{
    // Enable if any selected filesystem is stopped, or
    // if none is selected and at least one of the whole table is stopped
    start_btn->setEnabled(
        any_stopped()
            ||
        (
            refresh_fs_helpers::selected_rows_count(fs_table) == 0 
                &&
            any_stopped(true)
        )
    );

    // Enable if any selected filesystem is running, or
    // if none is selected and at least one of the whole table is running
    stop_btn->setEnabled(
        any_running()
            ||
        (
            refresh_fs_helpers::selected_rows_count(fs_table) == 0
                &&
            any_running(true)
        )
    );

    // Enable if any selected filesystem is not configured, or
    // if none is selected and at least one of the whole table is not configured
    setup_btn->setEnabled(
        at_least_one_configured(true, false)
            ||
        (
            refresh_fs_helpers::selected_rows_count(fs_table) == 0
                &&
            at_least_one_configured(true, true)
        )
    );

    // Enable if any of the selected fs is not in autostart
    add_autostart_btn->setEnabled(
        at_least_one_configured()
            &&
        any_selected_in_autostart(true)
    );

    remove_autostart_btn->setEnabled(
        at_least_one_configured()
            &&
        any_selected_in_autostart()
    );

    #ifdef BEEKEEPER_DEBUG_LOGGING
    // enable logs button only if exactly 1 row and it’s running with logging
    showlog_btn->setEnabled(
        refresh_fs_helpers::selected_rows_count(fs_table) == 1
            &&
        any_running_with_logging()
    );
    #endif

    // Enable only if more than 1 is selected,
    // none of them is running
    // and at least one is configured
    remove_btn->setEnabled(
        refresh_fs_helpers::selected_rows_count(fs_table) == 1
            &&
        at_least_one_configured()
            &&
        any_stopped()
    );
}

void
MainWindow::handle_start(bool enable_logging)
{
    if (!komander->do_i_have_root_permissions())
        return;

    auto *futures = new QList<QFuture<bool>>;

    // Decide which rows to process
    QList<QModelIndex> rows_to_process = fs_table->selectionModel()->selectedRows();
    if (rows_to_process.isEmpty()) {
        // No selection → consider all rows
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_process.append(fs_table->model()->index(r, 0));
    }

    for (auto idx : rows_to_process) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();

        // Discard running and unconfigured filesystems
        if (is_running(idx) || !is_configured(idx))
            continue;

        // Only create starting free space if it doesn't exist
        fs::path start_file = fs::path("/tmp") / ".beekeeper" / uuid.toStdString() / "startingfreespace";
        if (!fs::exists(start_file)) {
            fs::create_directories(start_file.parent_path());

            qint64 free_bytes = QString::fromStdString(
                komander->btrfstat(uuid.toStdString(), "free")
            ).toLongLong();

            std::ofstream ofs(start_file);
            if (ofs.is_open()) {
                ofs << free_bytes;
                ofs.close();
            }
        }

        // Queue the async start job
        futures->append(komander->async->beesstart(uuid, enable_logging));
    }

    // Hand over the queued futures for processing
    process_fs_async(futures);
}

void
MainWindow::handle_stop()
{
    if (!komander->do_i_have_root_permissions())
        return;

    // Decide which rows to process
    QList<QModelIndex> rows_to_process = fs_table->selectionModel()->selectedRows();
    if (rows_to_process.isEmpty()) {
        // No selection → consider all rows
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_process.append(fs_table->model()->index(r, 0));
    }

    // Use heap allocation so the futures survive after this function returns
    auto *futures = new QList<QFuture<bool>>;

    for (auto idx : rows_to_process) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();

        // Discard stopped filesystems
        if (is_stopped(idx))
            continue;

        // Discard unconfigured filesystems
        if (!is_configured(idx))
            continue;

        // Queue the async stop job
        futures->append(komander->async->beesstop(uuid));
    }

    // Hand over the queued futures for processing
    process_fs_async(futures);
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

    update_button_states();
}

void
MainWindow::handle_showlog()
{
    if (!komander->do_i_have_root_permissions()) {
        return;
    }

    if (refresh_fs_helpers::selected_rows_count(fs_table) != 1) {
        QMessageBox::warning(this,
                             tr("Show logs"),
                             tr("Please select only one filesystem to show its deduplication logs."));
        return;
    }

    auto idx = fs_table->selectionModel()->selectedRows().first();
    QString uuid   = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
    QString name   = fs_table->item(idx.row(), 1)->data(Qt::UserRole).toString();
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();

    // Always attempt to show logs, regardless of status
    QString logpath = QString::fromStdString(bk_mgmt::get_log_path(uuid.toStdString()));
    showLog(logpath, tr("Filesystem logs for: ") + name);

    update_button_states();
}


void
MainWindow::handle_remove_button()
{
    if (!komander->do_i_have_root_permissions())
        return;

    // Get selected filesystems
    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        QMessageBox::information(
            this,
            tr("No selection"),
            tr("Please select at least one filesystem to remove its Beesd configuration.")
        );
        return;
    }

    QList<QModelIndex> rows_to_process;

    // Progressive discard: skip running or unconfigured filesystems
    for (auto idx : selected_rows) {
        if (is_running(idx))
            continue;
        if (!is_configured(idx))
            continue;
        rows_to_process.append(idx);
    }

    if (rows_to_process.isEmpty()) {
        // Nothing to remove
        return;
    }

    // Confirm deletion dialog
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

    if (reply != QMessageBox::Yes)
        return;

    // Remove configs for surviving filesystems
    for (auto idx : rows_to_process) {
        std::string uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString().toStdString();
        std::string configfilepath = bk_util::trim_config_path_after_colon(
            komander->btrfstat(uuid, "")
        );
        if (!configfilepath.empty()) {
            komander->beesremoveconfig(uuid);
        }
    }

    // Refresh UI at the end
    refresh_filesystems();
    update_button_states();
}

void
MainWindow::process_fs_async(QList<QFuture<bool>> *futures)
{
    if (!futures || futures->isEmpty()) {
        delete futures;
        return;
    }

    auto remaining = new int(futures->size());
    auto success_count = new int(0);

    (void) QtConcurrent::run([this, futures, remaining, success_count]() {
        for (auto &f : *futures) {
            f.waitForFinished();
            if (f.result()) (*success_count)++;
            (*remaining)--;

            DEBUG_LOG(std::to_string(*remaining) + " futures remaining.");
        }

        QMetaObject::invokeMethod(this, [this, remaining, success_count, futures]() {
            refresh_filesystems();
            update_button_states();
            refresh_fs_helpers::update_status_manager(fs_table, statusManager);

            delete remaining;
            delete success_count;
            delete futures; // now safe to delete the heap list
        }, Qt::QueuedConnection);
    });
}
