#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "mainwindow.hpp"

#include <functional>

// Per-row checks

// Returns true if the filesystem in this row is running
bool
MainWindow::running (const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status.startsWith("running");
}

// Returns true if the filesystem in this row is running AND has logging enabled
bool
MainWindow::running_with_logging (const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status.startsWith("running") && status.contains("with logging");
}

// Returns true if the filesystem in this row is stopped
bool
MainWindow::stopped (const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status == "stopped";
}

// Returns true if the filesystem in this row is configured (not "unconfigured")
bool
MainWindow::configured (const QModelIndex &idx) const
{
    QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
    return status != "unconfigured";
}

bool
MainWindow::being_compressed(const QModelIndex &idx) const
{
    // Extract UUID from the table at column 0, stored in UserRole
    QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
    std::string uuid_std = uuid.toStdString();

    // Delegate check to management API
    return bk_mgmt::transparentcompression::is_running(uuid_std);
}

bool
MainWindow::in_the_autostart_file(const QModelIndex &idx) const
{
    // Extract UUID from the table at column 0, stored in UserRole
    QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
    std::string uuid_std = uuid.toStdString();

    // Delegate check to management API
    return bk_mgmt::autostart::is_enabled_for(uuid_std);
}





// ----- TABLE SCOPES -----



// Helper: build the list of QModelIndex to check (column 0 indices)
QModelIndexList
MainWindow::build_rows_to_check(const QTableWidget *table, bool check_the_whole_table)
{
    QModelIndexList rows;
    // If whole-table requested, add all rows
    if (check_the_whole_table) {
        for (int r = 0; r < table->rowCount(); ++r)
            rows.append(table->model()->index(r, 0));
        return rows;
    }

    // Otherwise, selected rows
    rows = table->selectionModel()->selectedRows();
    return rows;
}

// Internal implementation used by all four predicates
static bool evaluate_over_rows(const QTableWidget *table,
                               const QModelIndexList &rows,
                               const std::function<bool(const QModelIndex&)> &pred,
                               bool &out_any,       // true if at least one pred==true
                               bool &out_any_not,   // true if at least one pred==false
                               int &out_total_rows, // total rows evaluated
                               int &out_true_count)  // number of rows where pred==true
{
    out_any = false;
    out_any_not = false;
    out_total_rows = 0;
    out_true_count = 0;

    for (const QModelIndex &idx : rows) {
        ++out_total_rows;
        bool r = false;
        try {
            r = pred(idx);
        } catch (...) {
            // Be defensive: treat exceptions as 'false' for this row
            r = false;
        }

        if (r) {
            out_any = true;
            ++out_true_count;
        } else {
            out_any_not = true;
        }
    }

    return true;
}

// Public API implementations --------------------------

bool
MainWindow::is_any(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table) const
{
    QModelIndexList rows = build_rows_to_check(fs_table, check_the_whole_table);
    if (rows.isEmpty()) return false;

    bool any = false, any_not = false;
    int total = 0, true_count = 0;
    evaluate_over_rows(fs_table, rows, func, any, any_not, total, true_count);
    return any;
}

bool
MainWindow::is_any_not(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table) const
{
    QModelIndexList rows = build_rows_to_check(fs_table, check_the_whole_table);
    if (rows.isEmpty()) return false;

    bool any = false, any_not = false;
    int total = 0, true_count = 0;
    evaluate_over_rows(fs_table, rows, func, any, any_not, total, true_count);
    return any_not;
}

bool
MainWindow::is_none(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table) const
{
    QModelIndexList rows = build_rows_to_check(fs_table, check_the_whole_table);
    if (rows.isEmpty()) return true; // vacuously none

    bool any = false, any_not = false;
    int total = 0, true_count = 0;
    evaluate_over_rows(fs_table, rows, func, any, any_not, total, true_count);
    return (true_count == 0);
}

bool
MainWindow::are_all(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table) const
{
    QModelIndexList rows = build_rows_to_check(fs_table, check_the_whole_table);
    if (rows.isEmpty()) return false;

    bool any = false, any_not = false;
    int total = 0, true_count = 0;
    evaluate_over_rows(fs_table, rows, func, any, any_not, total, true_count);
    return (true_count == total && total > 0);
}

// Overloads for pointer-to-member (so calling is_any(running) from inside the class works)
bool
MainWindow::is_any(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table) const
{
    return is_any([this, mf](const QModelIndex &idx){ return (this->*mf)(idx); }, check_the_whole_table);
}

bool
MainWindow::is_any_not(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table) const
{
    return is_any_not([this, mf](const QModelIndex &idx){ return (this->*mf)(idx); }, check_the_whole_table);
}

bool
MainWindow::is_none(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table) const
{
    return is_none([this, mf](const QModelIndex &idx){ return (this->*mf)(idx); }, check_the_whole_table);
}

bool
MainWindow::are_all(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table) const
{
    return are_all([this, mf](const QModelIndex &idx){ return (this->*mf)(idx); }, check_the_whole_table);
}
