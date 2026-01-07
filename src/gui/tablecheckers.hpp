#pragma once
#include "beekeeper/internalaliases.hpp"
#include <QModelIndex>
#include <QTableWidget>

namespace tablecheckers {
        // ----- Table checkers - per entire selection -----

    /**
    * Check if any filesystem is in a certain state.
    *
    * By default, these functions only check the status of selected rows.
    * See in the block right below this one what the possible states are
    * (spoiler: running, stopped, configured, being_compressed, etc.0)
    * If `check_the_whole_table_if_not_selected` is true, they will check
    * the status of all rows in the table if none is actually selected
    *
    * @param check_the_whole_table If true, include all rows in the table; otherwise, only selected rows are checked.
    * @return true if at least one filesystem matches the condition, false otherwise.
    */
    // Generic row-testing helpers (operate on selected rows by default,
    // otherwise whole table if check_the_whole_table is true).
    bool is_any(std::function<bool(const QModelIndex&, const fs_map&)> predicate, const QTableWidget *table, const fs_map &source_of_truth, bool check_the_whole_table_if_none_selected = false);
    bool is_any_not(std::function<bool(const QModelIndex&, const fs_map&)> predicate, const QTableWidget *table, const fs_map &source_of_truth, bool check_the_whole_table_if_none_selected = false);
    bool is_none(std::function<bool(const QModelIndex&, const fs_map&)> predicate, const QTableWidget *table, const fs_map &source_of_truth, bool check_the_whole_table_if_none_selected = false);
    bool are_all(std::function<bool(const QModelIndex&, const fs_map&)> predicate, const QTableWidget *table, const fs_map &source_of_truth, bool check_the_whole_table_if_none_selected = false);

    // Gets the selected table items and gets all the table items if check_the_whole_table is true.
    QModelIndexList list_of_selected_rows (const QTableWidget *table, bool check_the_whole_table_if_none_selected);
    QString fetch_user_role(const QModelIndex &idx, int column);



    // ----- Table-checkers - per row checks -----

    // Returns true if the filesystem in this row is running
    bool running(const QModelIndex &idx, const fs_map &source_of_truth);

    // Returns true if the filesystem in this row is running AND has logging enabled
    bool running_with_logging(const QModelIndex &idx, const fs_map &source_of_truth);

    // Returns true if the filesystem in this row is stopped
    bool stopped(const QModelIndex &idx, const fs_map &source_of_truth);

    // Returns true if the filesystem in this row is configured (not "unconfigured")
    bool configured(const QModelIndex &idx, const fs_map &source_of_truth);

    // Returns true if the filesystem in this row has a compress= mount option
    bool being_compressed(const QModelIndex &idx, const fs_map &source_of_truth);

    // Returns true if the filesystem is in the beekeeper autostart file
    bool in_the_autostart_file(const QModelIndex &idx, const fs_map &source_of_truth);


    
    // fetch info from our source of truth for this uuid
    fs_info info_for_row(const QModelIndex &idx, const fs_map &source_of_truth);
}