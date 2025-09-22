#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "mainwindow.hpp"
#include "../polkit/multicommander.hpp"
#include "refreshfilesystems_helpers.hpp"
#include <filesystem>
#include <QGraphicsColorizeEffect>
#include <QMessageBox>
#include <QPushButton>
#include <string>

namespace fs = std::filesystem;

// -------------------------
void
MainWindow::update_button_states()
{
    using mw = MainWindow;

    // Enable if any selected filesystem is stopped, or
    // if none is selected and at least one of the whole table is stopped
    start_btn->setEnabled(
        is_any(&mw::stopped)
            ||
        (
            refresh_fs_helpers::selected_rows_count(fs_table) == 0
                &&
            is_any(&mw::stopped, true)
        )
    );

    // Enable if any selected filesystem is running, or
    // if none is selected and at least one of the whole table is running
    stop_btn->setEnabled(
        is_any(&mw::running)
            ||
        (
            refresh_fs_helpers::selected_rows_count(fs_table) == 0
                &&
            is_any(&mw::running, true)
        )
    );

    // Enable if any selected filesystem is not configured, or
    // if none is selected and at least one of the whole table is not configured
    setup_btn->setEnabled(
        is_any_not(&mw::configured)
            ||
        (
            refresh_fs_helpers::selected_rows_count(fs_table) == 0
                &&
            is_any_not(&mw::configured, true)
        )
    );

    // Enable if any of the selected fs is not in autostart
    add_autostart_btn->setEnabled(
        is_any(&mw::configured)
            &&
        is_any_not(&mw::in_the_autostart_file)
    );

    remove_autostart_btn->setEnabled(
        is_any(&mw::configured)
            &&
        is_any(&mw::in_the_autostart_file)
    );

    #ifdef BEEKEEPER_DEBUG_LOGGING
    // enable logs button only if exactly 1 row and it’s running with logging
    showlog_btn->setEnabled(
        refresh_fs_helpers::selected_rows_count(fs_table) == 1
            &&
        is_any(&mw::running_with_logging)
    );
    #endif

    // Enable only if exactly 1 is selected,
    // it is configured and stopped
    remove_btn->setEnabled(
        refresh_fs_helpers::selected_rows_count(fs_table) == 1
            &&
        is_any(&mw::configured)
            &&
        is_any(&mw::stopped)
    );

    // -------------------------
    // Transparent-compression switch button state (refactor)
    // -------------------------
    if (compression_switch_btn) {
        static QIcon base_icon = QIcon::fromTheme("package-x-generic");
        compression_switch_btn->setIcon(base_icon);

        int sel_count = refresh_fs_helpers::selected_rows_count(fs_table);

        // Determine scope: if nothing selected, consider whole table
        bool check_whole_table = (sel_count == 0);

        using mw = MainWindow;
        bool any_not_compressed = is_any_not(&mw::being_compressed, check_whole_table);
        bool any_compressed     = is_any(&mw::being_compressed, check_whole_table);

        // Decide visual state:
        // - If ANY filesystem in scope is NOT being compressed => we show the button PRESSED
        //   because user can click to *pause* (the button represents the "freeze/stop" action).
        // - Otherwise (all running) show UNPRESSED (normal color).
        bool should_be_checked = any_not_compressed;

        // Temporarily block signals so setChecked(...) does NOT trigger the handler.
        {
            QSignalBlocker blocker(compression_switch_btn);
            compression_switch_btn->setChecked(should_be_checked);
        }

        // Tooltip + effect depend on the "paused" (checked) state
        if (should_be_checked) {
            // Paused (button pressed) — grayscale to indicate "frozen/paused"
            compression_switch_btn->setToolTip(tr("Transparent compression stopped for selected filesystems"));
        } else {
            // Running (unpressed) — full color
            compression_switch_btn->setToolTip(tr("Transparent compression running for selected filesystems"));
        }
    }
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
        if (running(idx) || !configured(idx))
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
        if (stopped(idx))
            continue;

        // Discard unconfigured filesystems
        if (!configured(idx))
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
MainWindow::handle_transparentcompression_switch(bool pause)
{
    using namespace beekeeper::privileged;
    namespace tc = bk_mgmt::transparentcompression;

    if (!komander->do_i_have_root_permissions()) {
        return; // No root → nothing to do
    }

    // Build the rows we want to consider (selection or whole table if empty)
    QModelIndexList rows_to_process =
        build_rows_to_check(fs_table,
                            fs_table->selectionModel()->selectedRows().isEmpty());

    if (rows_to_process.isEmpty()) {
        return; // Nothing to do
    }

    // Create futures container on the heap so process_fs_async can own it
    auto *futures = new QList<QFuture<bool>>();

    for (const QModelIndex &idx : rows_to_process) {
        QTableWidgetItem *uuid_item = fs_table->item(idx.row(), 0);
        if (!uuid_item) continue;

        QString uuid_q = uuid_item->data(Qt::UserRole).toString();
        if (uuid_q.isEmpty()) continue;

        std::string uuid_std = uuid_q.toStdString();

        // Skip if compression not enabled for this uuid
        if (!tc::is_enabled_for(uuid_std)) {
            continue;
        }

        // Check actual compression state of this row
        bool currently_compressed = being_compressed(idx);

        if (pause) {
            // Pause only if currently compressed
            if (currently_compressed) {
                futures->append(
                    komander->async->pause_transparentcompression_for_uuid(uuid_q)
                );
            }
        } else {
            // Start only if not currently compressed
            if (!currently_compressed) {
                futures->append(
                    komander->async->start_transparentcompression_for_uuid(uuid_q)
                );
            }
        }
    }

    if (futures->isEmpty()) {
        delete futures;
        return;
    }

    // Fire and forget: process async futures
    process_fs_async(futures);
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
        if (running(idx))
            continue;
        if (!configured(idx))
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
    // Remove configs for surviving filesystems
    for (auto idx : rows_to_process) {
        std::string uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString().toStdString();
        std::string configfilepath = bk_util::trim_config_path_after_colon(
            komander->btrfstat(uuid, "")
        );
        if (!configfilepath.empty()) {
            // Remove Beesd configuration
            komander->beesremoveconfig(uuid);

            // Also remove it from autostart
            if (bk_mgmt::autostart::is_enabled_for(uuid)) {
                komander->remove_uuid_from_autostart(uuid);
            }

            // Also remove from transparent compression if enabled
            if (bk_mgmt::transparentcompression::is_enabled_for(uuid)) {
                komander->remove_uuid_from_transparentcompression(uuid);
            }
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
        const int timeout_ms = 30'000;       // 30 seconds per future
        const int poll_interval_ms = 50;     // poll every 50 ms

        for (auto &f : *futures) {
            QElapsedTimer timer;
            timer.start();

            // Poll until finished or timeout
            while (!f.isFinished() && timer.elapsed() < timeout_ms) {
                QThread::msleep(poll_interval_ms);
            }

            if (f.isFinished()) {
                if (f.result()) (*success_count)++;
            } else {
                DEBUG_LOG("Future timed out!");
            }

            (*remaining)--;
            DEBUG_LOG(std::to_string(*remaining) + " futures remaining.");
        }

        // Post back to main thread safely
        QMetaObject::invokeMethod(this, [this, remaining, success_count, futures]() {
            emit command_finished();

            delete remaining;
            delete success_count;
            delete futures;
        }, Qt::QueuedConnection);
    });
}

