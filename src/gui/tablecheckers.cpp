#include "mainwindow.hpp"

// Checks on the whole selections

bool
MainWindow::any_running(bool check_the_whole_table) const
{
    auto rows_to_check = fs_table->selectionModel()->selectedRows();
    if (check_the_whole_table) {
        rows_to_check.clear();
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_check.append(fs_table->model()->index(r, 0));
    }

    for (auto idx : rows_to_check) {
        if (is_running(idx)) {
            return true;
        }
    }
    return false;
}

bool
MainWindow::any_running_with_logging(bool check_the_whole_table) const
{
    auto rows_to_check = fs_table->selectionModel()->selectedRows();
    if (check_the_whole_table) {
        rows_to_check.clear();
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_check.append(fs_table->model()->index(r, 0));
    }

    for (auto idx : rows_to_check) {
        if (is_running_with_logging(idx)) {
            return true;
        }
    }
    return false;
}

bool
MainWindow::any_stopped(bool check_the_whole_table) const
{
    auto rows_to_check = fs_table->selectionModel()->selectedRows();
    if (check_the_whole_table) {
        rows_to_check.clear();
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_check.append(fs_table->model()->index(r, 0));
    }

    for (auto idx : rows_to_check) {
        if (is_stopped(idx)) {
            return true;
        }
    }
    return false;
}

// Return true if at least one explicitly selected filesystem is configured.
// If invert is true, return true if at least one selected filesystem is unconfigured instead.
bool
MainWindow::at_least_one_configured(bool invert, bool check_the_whole_table) const
{
    auto rows_to_check = fs_table->selectionModel()->selectedRows();
    if (check_the_whole_table) {
        rows_to_check.clear();
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_check.append(fs_table->model()->index(r, 0));
    }

    for (auto idx : rows_to_check) {
        if (invert? !is_configured(idx) : is_configured(idx)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if any of the selected filesystems are in the autostart list.
 *
 * @param reverse If true, inverts the logic:
 *        returns false if any selected filesystem is in autostart,
 *        otherwise returns true.
 *
 * @return true if the condition matches, false otherwise.
 */
bool MainWindow::any_selected_in_autostart(bool reverse)
{
    QModelIndexList selected = fs_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return false;

    for (const QModelIndex &idx : selected) {
        if (!is_configured(idx))
            continue;

        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        std::string uuid_std = uuid.toStdString();
        // DEBUG_LOG("Autostart list: ", bk_util::serialize_vector(bk_util::list_uuids_in_autostart()));
        bool in_autostart = bk_util::is_uuid_in_autostart(uuid_std);

        if (in_autostart)
            return (!reverse ? true : false);
    }

    return (!reverse ? false : true);
}

// Per-row checks

// Returns true if the filesystem in this row is running
bool
MainWindow::is_running(const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status.startsWith("running");
}

// Returns true if the filesystem in this row is running AND has logging enabled
bool
MainWindow::is_running_with_logging(const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status.startsWith("running") && status.contains("with logging");
}

// Returns true if the filesystem in this row is stopped
bool
MainWindow::is_stopped(const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status == "stopped";
}

// Returns true if the filesystem in this row is configured (not "unconfigured")
bool
MainWindow::is_configured(const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status != "unconfigured";
}

