#include "beekeeper/transparentcompressionmgmt.hpp"
#include "mainwindow.hpp"
#include "../polkit/globals.hpp"
#include "../polkit/_staticcommander.hpp"
#include "refreshfilesystems_helpers.hpp"
#include "tablecheckers.hpp"
#include <QFuture>
#include <QGraphicsColorizeEffect>
#include <QMessageBox>
#include <QPushButton>
#include <qabstractitemmodel.h>
#include <unordered_map>

// So we can use the asynchronic versions as predicates

namespace neokomander = beekeeper::privileged::_static;

using namespace tablecheckers;

// -------------------------
void
MainWindow::update_button_states()
{
    using mw = MainWindow;

    // Enable if any selected filesystem is stopped, or
    // if none is selected and at least one of the whole table is stopped
    start_btn->setEnabled(
        is_any(stopped, fs_table, fs_view_state, true)
    );

    // Enable if any selected filesystem is running, or
    // if none is selected and at least one of the whole table is running
    stop_btn->setEnabled(
        is_any(running, fs_table, fs_view_state, true)
    );

    // Enable if any selected filesystem is not configured, or
    // if none is selected and at least one of the whole table is not configured
    setup_btn->setEnabled(
        is_any_not(configured, fs_table, fs_view_state, false)
    );

    // Enable if any of the selected fs is not in autostart
    add_autostart_btn->setEnabled(
        is_any(configured, fs_table, fs_view_state, true)
            &&
        is_any_not(in_the_autostart_file, fs_table, fs_view_state)
    );

    remove_autostart_btn->setEnabled(
        is_any(configured, fs_table, fs_view_state)
            &&
        is_any(in_the_autostart_file, fs_table, fs_view_state)
    );

    #ifdef BEEKEEPER_DEBUG_LOGGING
    // enable logs button only if exactly 1 row and it’s running with logging
    showlog_btn->setEnabled(
        refresh_fs_helpers::selected_rows_count(fs_table) == 1
            &&
        is_any(running_with_logging, fs_table, fs_view_state)
    );
    #endif

    // Enable only if exactly 1 is selected,
    // it is configured and stopped
    remove_btn->setEnabled(
        refresh_fs_helpers::selected_rows_count(fs_table) == 1
            &&
        is_any(configured, fs_table, fs_view_state)
            &&
        is_any(stopped, fs_table, fs_view_state)
    );

    // -------------------------
    // Transparent-compression switch button state (refactor)
    // -------------------------
    if (compression_switch_btn) {
        static QIcon base_icon = QIcon::fromTheme("package-x-generic");
        compression_switch_btn->setIcon(base_icon);

        int sel_count = refresh_fs_helpers::selected_rows_count(fs_table);;

        using mw = MainWindow;
        bool any_not_compressed = is_any_not(being_compressed, fs_table, fs_view_state, true);
        bool any_compressed     = is_any(being_compressed, fs_table, fs_view_state, true);

        // Decide visual state:
        // - If ANY filesystem in scope is NOT being compressed => we show the button PRESSED
        //   because user can click to *pause* (the button represents the "freeze/stop" action).
        // - Otherwise (all running) show UNPRESSED (normal color).

        // In other words: always prefer to start the compression rather than stopping it
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
MainWindow::handle_start (bool enable_logging)
{
    if (!komander->do_i_have_root_permissions())
        return;

    set_temporal_status_message(tr("Starting") + "...", 3000);

    // Decide which rows to process
    // No selection → consider all rows
    const QModelIndexList selected = list_of_selected_rows(fs_table, true);

    // Discard if...
    std::vector<std::function<bool(const QModelIndex &)>> discard_if = {
        [this](const QModelIndex &idx) { return running(idx, fs_view_state); },

        // or

        // not_configured
        [this](const QModelIndex &idx) { return !configured(idx, this->fs_view_state); }
    };


    futuristically_process_indices_with_predicate(
        // what to do
        neokomander::beesstart,

        // discard_if
        discard_if,

        // for what
        selected,

        // and what else
        enable_logging
    );

    // Update status in the GUI
    /* Let's be optimistic, but first, let's be real: if something goes
    * wrong here, the Status column on the GUI will catch it anyway and
    * the refresh cycle will fix this with refreshing ice cold water like
    * 5 seconds later.
    * This holds true for all of the following functions.
    */
    for (const auto &idx : selected) {
        fs_view_state[
            // for this UUID...
            refresh_fs_helpers::fetch_user_role(idx, 0).toStdString()

            // change status of something to...
        ].status = "running";
    }
}

void
MainWindow::handle_stop()
{
    if (!komander->do_i_have_root_permissions())
        return;

    set_temporal_status_message(tr("Stopping") + "...", 3000);

    const QModelIndexList selected = list_of_selected_rows(fs_table, true);

    std::vector<std::function<bool(const QModelIndex &)>> discard_if = {
        // not running
        [this](const QModelIndex &idx) { return !running(idx, fs_view_state); },

        // or not configured
        [this](const QModelIndex &idx) { return !configured(idx, fs_view_state); }
    };

    futuristically_process_indices_with_predicate(
        neokomander::beesstop,
        discard_if,
        selected
    );

    for (const auto &idx : selected) {
        fs_view_state[
            // for this UUID...
            refresh_fs_helpers::fetch_user_role(idx, 0).toStdString()

            // change status of something to...
        ].status = "stopped";
    }
}

void
MainWindow::handle_setup()
{
    if (!komander->do_i_have_root_permissions()) return;

    // No selection → consider all rows
    const QModelIndexList selected = list_of_selected_rows(fs_table, true);

    // this does not directly throw a neokomander predicate, just hands
    // over to the setup dialog

    QStringList uuids_to_setup;

    for (const auto &idx : selected) {
        uuids_to_setup.push_back(refresh_fs_helpers::fetch_user_role(idx, 0));
    }

    if (!uuids_to_setup.isEmpty()) {
        // SetupDialog internally filters only unconfigured UUIDs
        SetupDialog dlg(uuids_to_setup, this);
        dlg.exec();
    }

    // will change the status in the Setup dialog instead
}

void
MainWindow::handle_showlog()
{
    if (!komander->do_i_have_root_permissions()) {
        return;
    }

    // no-op: logging through GUI doesn't exist anymore
}

void
MainWindow::handle_transparentcompression_switch(bool pause)
{
    using namespace beekeeper::privileged;
    namespace tc = bk_mgmt::transparentcompression;

    if (!komander->do_i_have_root_permissions())
        return;

    const QModelIndexList selected =
        list_of_selected_rows(fs_table, true);

    // record uuids and what was their last compression status
    std::unordered_map<std::string, bool> did_this_uuid_have_compression_running;
    for (const auto &idx : selected) {
        // what uuid?
        std::string uuid = refresh_fs_helpers::fetch_user_role(idx, 0).toStdString();

        did_this_uuid_have_compression_running.emplace(
            // record such info for this uuid
            uuid,

            // actually know if it had it enabled
            tc::is_running(uuid)
        );
    }

    std::vector<std::function<bool(const QModelIndex &)>> discard_if = {
        // no discard rules
    };

    // toggle transparent compression
    auto predicate =
    [this, pause, did_this_uuid_have_compression_running](const QString &uuid) -> QFuture<bool>
    {
        if (!did_this_uuid_have_compression_running.at(uuid.toStdString()) && !pause) {
            // wants to start
            return komander->start_transparentcompression_for_uuid(uuid);
        }

        if (did_this_uuid_have_compression_running.at(uuid.toStdString()) && pause) {
            // wants to stop
            return komander->pause_transparentcompression_for_uuid(uuid);
        }

        // no-op: wrap a ready future with false
        QPromise<bool> promise;
        promise.start();
        promise.addResult(false);
        promise.finish();
        return promise.future();
    };

    futuristically_process_indices_with_predicate(
        predicate,
        discard_if,
        selected
    );

    // change visible status on table
    for (const auto &idx : selected) {
        // what uuid?
        std::string uuid = refresh_fs_helpers::fetch_user_role(idx, 0).toStdString();

        fs_view_state[
            // for this UUID...
            uuid

            // change status of something to...
        ].compressing = !did_this_uuid_have_compression_running.at(uuid);
    }
}


void
MainWindow::handle_remove_button()
{
    if (!komander->do_i_have_root_permissions())
        return;

    const QModelIndexList selected =
        list_of_selected_rows(fs_table, false);

    if (selected.isEmpty()) {
        QMessageBox::information(
            this,
            tr("No selection"),
            tr("Please select at least one filesystem to remove its Beesd configuration.")
        );
        return;
    }

    QString message =
        tr("You are about to remove the file deduplication engine configuration for the selected filesystem(s).\n")
        + tr("This does not cause any data loss, but you will lose deduplication functionality until you set up Beesd again.\n\n")
        + tr("Do you want to continue?");

    if (QMessageBox::question(
            this,
            tr("Confirm configuration removal"),
            message,
            QMessageBox::Yes | QMessageBox::No
        ) != QMessageBox::Yes)
    {
        return;
    }

    std::vector<std::function<bool(const QModelIndex &)>> discard_if = {
        // discard if running
        [this](const QModelIndex &idx) { return running(idx, fs_view_state); },

        // or not configured
        [this](const QModelIndex &idx) { return !configured(idx, fs_view_state); }
    };

    futuristically_process_indices_with_predicate(
        neokomander::beesremoveconfig,
        discard_if,
        selected
    );

    // mark it as unconfigured and empty the config path
    for (const auto &idx : selected) {
        std::string uuid = refresh_fs_helpers::fetch_user_role(idx, 0).toStdString();
        fs_view_state[
            // for this UUID...
            uuid

            // change status of something to...
        ].status = "unconfigured";
        fs_view_state[
            uuid
        ].config = "";
    }
}
