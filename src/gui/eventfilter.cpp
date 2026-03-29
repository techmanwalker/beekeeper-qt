#pragma once

#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "tablecheckers.hpp"
#include "mainwindow.hpp"

/**
 * @brief Event filter for hover detection over the filesystem table.
 * 
 * Monitors mouse movement over fs_table's viewport to detect when the user
 * hovers over a filesystem row. When hovering a running deduplication job,
 * displays space savings info in the status bar.
 * 
 * @param obj The object receiving the event
 * @param event The event to process
 * @return true if event was handled, false to pass to parent
 */
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Step 1: Check if this is a mouse move event on the table's viewport
    // The viewport is the actual painted area containing table cells
    if (obj != fs_table->viewport() || event->type() != QEvent::MouseMove) {
        return QMainWindow::eventFilter(obj, event);
    }

    // Step 2: Cast to mouse event and get position relative to viewport
    auto *mouseEvent = static_cast<QMouseEvent*>(event);
    QPoint pos = mouseEvent->pos();

    // Step 3: Convert pixel position to model index (row/column)
    // indexAt returns invalid index if mouse is between rows or outside table
    QModelIndex idx = fs_table->indexAt(pos);

    // Step 4: Handle hover exit - clear status bar when not over any item
    if (!idx.isValid()) {
        barmessage->print("");
        return QMainWindow::eventFilter(obj, event);
    }

    // Step 5: Extract UUID from the hovered row's user role data
    // Column 0 contains the UUID delegate; UserRole stores the raw UUID string
    QString uuid = refresh_fs_helpers::fetch_user_role(idx, 0);

    // Step 6: Construct path to the "starting free space" record file
    // This file is created when deduplication starts, recording initial disk state
    std::string space_record_path = bk_mgmt::started_with_n_gb_file_path(uuid.toStdString());

    // Step 7: Read the historical free space value from the record file
    // If file doesn't exist (job never started), this will be 0 or fail silently
    double starting_free_space = 0.0;
    std::ifstream space_file(space_record_path);
    if (space_file.is_open()) {
        space_file >> starting_free_space;
        space_file.close();
    }

    // Step 8: Query current filesystem free space in real-time
    // This calls into the backend to get live disk statistics
    double current_free_space = bk_mgmt::get_space::free(uuid.toStdString());

    // Step 9: Check if deduplication is actively running on this filesystem
    // running() examines fs_view_state to determine operational status
    if (tablecheckers::running(idx, fs_view_state)) {
        // Step 10: Format and display the space savings message
        // auto_size_suffix converts bytes to human-readable (GB, TB, etc.)
        barmessage->print(
            tr("Deduplicating files. Started with %1 free, now you have %2 free.")
                .arg(bk_util::auto_size_suffix(starting_free_space))
                .arg(bk_util::auto_size_suffix(current_free_space))
        );
    } else {
        // Not running - clear any previous message
        barmessage->print("");
    }

    return QMainWindow::eventFilter(obj, event);
}