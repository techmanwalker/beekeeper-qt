#include "tablecheckers.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "tablecheckers.hpp"
#include "refreshfilesystems_helpers.hpp"

#include <functional>
#include <qabstractitemmodel.h>
#include <qtablewidget.h>

// Helpers, as always

fs_info
tablecheckers::info_for_row(const QModelIndex &idx, const fs_map &source_of_truth)
{
    QString uuid_qstr = refresh_fs_helpers::fetch_user_role(idx, 0);
    if (uuid_qstr.isEmpty()) {
        return fs_info {};
        DEBUG_LOG("Returned an empty fs_info because the uuid string was empty.");
    }

    auto it = source_of_truth.find(uuid_qstr.toStdString());
    if (it == source_of_truth.end()) {
        DEBUG_LOG("Returned an empty fs_info because the required uuid ", uuid_qstr, " wasn't present on the lookup fs_map.");
        return fs_info {};
    }

    fs_info info = it->second;

     #ifdef BEEKEEPER_DEBUG_LOGGING
    DEBUG_LOG("Filesystem info for uuid,", uuid_qstr, " according to fs_table_view: \n",
        "  uuid=", uuid_qstr, "\n",
        "  label=", info.label, "\n",
        "  status=", info.status, "\n",
        "  devname=", info.devname, "\n",
        "  config=", info.config, "\n",
        "  compressing=", info.compressing, "\n",
        "  autostart=", info.autostart, "\n"
    );
    #endif
    return info;
}

// Per-row checks

// Returns true if the filesystem in this row is running
bool
tablecheckers::running(const QModelIndex &idx, const fs_map &source_of_truth)
{
    const fs_info info = info_for_row(idx, source_of_truth);
    

    bool res = info.status.find("running") != std::string::npos;

    return res;
}

// Returns true if the filesystem in this row is running AND has logging enabled
bool
tablecheckers::running_with_logging(const QModelIndex &idx, const fs_map &source_of_truth)
{
    const fs_info info = info_for_row(idx, source_of_truth);
    

    bool res = info.status.find("running") != std::string::npos
        && info.status.find("with logging") != std::string::npos;

    return res;
}

// Returns true if the filesystem in this row is stopped
bool
tablecheckers::stopped(const QModelIndex &idx, const fs_map &source_of_truth)
{
    const fs_info info = info_for_row(idx, source_of_truth);

    bool res = info.status == "stopped";

    return res;
}

// Returns true if the filesystem in this row is configured (not "unconfigured")
bool
tablecheckers::configured(const QModelIndex &idx, const fs_map &source_of_truth)
{
    const fs_info info = info_for_row(idx, source_of_truth);

    bool res = info.config != "";

    return res;
}

bool
tablecheckers::being_compressed(const QModelIndex &idx, const fs_map &source_of_truth)
{
    const fs_info info = info_for_row(idx, source_of_truth);

    bool res = info.compressing;

    return res;
}

bool
tablecheckers::in_the_autostart_file(const QModelIndex &idx, const fs_map &source_of_truth)
{
    const fs_info info = info_for_row(idx, source_of_truth);

    bool res = info.autostart;

    return res;
}





// ----- TABLE SCOPES -----



// Helper: build the list of QModelIndex to check (column 0 indices)
QModelIndexList
tablecheckers::list_of_selected_rows(const QTableWidget *table, bool check_the_whole_table_if_none_selected)
{
    QModelIndexList rows;
    // Otherwise, selected rows
    rows = table->selectionModel()->selectedRows(0);

    // If whole-table requested, mark all rows as "virtually selected"
    if (check_the_whole_table_if_none_selected && rows.count() == 0) {
        rows.reserve(table->rowCount());
        for (int r = 0; r < table->rowCount(); ++r)
            rows.append(table->model()->index(r, 0));
        return rows;
    }

    return rows;
}

// Public API implementations --------------------------

/* evaluate each idx of selection like predicate(idx)
* so e.g. calling is_any(running) evaluates
* runnnig(idx) for every virtually selected item
* this holds true for all next is_* and are_* checkers
*/ 

bool
tablecheckers::is_any(std::function<bool(const QModelIndex&, const fs_map&)> predicate,
                    const QTableWidget *table,
                    const fs_map &source_of_truth,
                    bool check_the_whole_table_if_none_selected)
{
    QModelIndexList selection =
        list_of_selected_rows(table, check_the_whole_table_if_none_selected);

    // return true if at least one of the item's predicate
    // evaluation returns true

    for (const QModelIndex &idx : selection) {
        if (predicate(idx, source_of_truth))
            return true;
    }

    return false;
}

bool
tablecheckers::is_any_not(std::function<bool(const QModelIndex&, const fs_map&)> predicate,
                        const QTableWidget *table,
                        const fs_map &source_of_truth,
                        bool check_the_whole_table_if_none_selected)
{
    QModelIndexList selection =
        list_of_selected_rows(table, check_the_whole_table_if_none_selected);

    // return true if at least one of the item's predicate
    // evaluation returns false

    for (const QModelIndex &idx : selection) {
        if (!predicate(idx, source_of_truth))
            return true;
    }

    return false;
}

bool
tablecheckers::is_none(std::function<bool(const QModelIndex&, const fs_map&)> predicate,
                    const QTableWidget *table,
                    const fs_map &source_of_truth,
                    bool check_the_whole_table_if_none_selected)
{
    QModelIndexList selection =
        list_of_selected_rows(table, check_the_whole_table_if_none_selected);

    // return true if absolutely all of the item's predicate
    // evaluations return false

    for (const QModelIndex &idx : selection) {
        if (predicate(idx, source_of_truth))
            return false;
    }

    return true;
}

// Note: the GUI table is only used to fetch selection.
// The source_of_truth is the actual data source that we use to check the
// filesystem status.
bool
tablecheckers::are_all(std::function<bool(const QModelIndex&, const fs_map&)> predicate,
                    const QTableWidget *table,
                    const fs_map &source_of_truth,
                    bool check_the_whole_table_if_none_selected)
{
    QModelIndexList selection =
        list_of_selected_rows(table, check_the_whole_table_if_none_selected);

    // return true if absolutely all of the item's predicate
    // evaluations return true

    for (const QModelIndex &idx : selection) {
        if (!predicate(idx, source_of_truth))
            return false;
    }

    return true;
}