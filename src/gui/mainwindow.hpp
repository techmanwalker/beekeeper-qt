#pragma once

#include "dedupstatusmanager.hpp"
#include "keyboardnav.hpp"
#include "rootshellthread.hpp"
#include "setupdialog.hpp"
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <vector>

// Forward declarations
class StatusDotDelegate;
class UUIDColumnDelegate;

// Type aliases
using fs_map = std::map<std::string, std::string>;
using fs_vec = std::vector<fs_map>;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class KeyboardNav;

public:
   
    explicit
    MainWindow(QWidget *parent = nullptr);

    // root operations
    void set_root_thread(root_shell_thread *thread);

    QString map_status_text(const QString &raw_status);

    // Set a temporal message on the status bar for an amount of time
    void set_temporal_status_message(const QString message, qint64 duration_in_ms);

    friend class SetupDialog;

private:
    void setup_ui();
    void refresh_filesystems();

    // Button handlers - handles the toolbar buttons

    void handle_start(bool enable_logging = false);
    void handle_stop();
    void handle_setup();
    void handle_showlog();
    void handle_transparentcompression_switch(bool pause);
    void handle_remove_button();
    void handle_cpu_timer();
    void update_statuses();
    void update_status_bar();

    // ----- Add or remove your filesystems from autostart -----

    void handle_add_to_autostart();
    void handle_remove_from_autostart();

    /** The star function - handles all the button handlers
    * @brief Processes a batch of asynchronous filesystem commands.
    *
    * Each toolbar button (Start, Stop, etc.) prepares its own list of QFuture<bool> tasks corresponding
    * to the selected (or all) filesystems. These tasks represent operations that may take time, like
    * starting or stopping deduplication daemons.
    *
    * The purpose of this function is to **wait for all provided asynchronous tasks to complete** without
    * blocking the GUI thread. Once all tasks have finished, it performs the necessary updates:
    *   - refresh_filesystems() indirectly via update_status_manager()
    *   - update_status_manager() to update the status manager UI
    *   - update_button_states() to reflect the new state of the filesystems
    *
    * Handlers (e.g., handle_start(), handle_stop()) are responsible for:
    *   - Deciding which filesystems to operate on
    *   - Creating the QFuture<bool> objects representing the asynchronous operations
    *   - Passing the list of futures to this function
    *
    * @param futures A list of QFuture<bool> tasks representing pending asynchronous operations.
    *                This list may be empty; in that case, the function returns immediately.
    */
    template <typename T> void process_fs_async(QList<QFuture<T>> *futures);


    // ----- Table checkers - per entire selection -----

    /**
    * Check if any filesystem is in a certain state.
    *
    * By default, these functions only check the status of selected rows.
    * See in the block right below this one what the possible states are
    * (spoiler: running, stopped, configured, being_compressed, etc.0)
    * If `check_the_whole_table` is true, they will check the status of
    * all rows in the table instead of just the selection.
    *
    * @param check_the_whole_table If true, include all rows in the table; otherwise, only selected rows are checked.
    * @return true if at least one filesystem matches the condition, false otherwise.
    */
    // Generic row-testing helpers (operate on selected rows by default,
    // otherwise whole table if check_the_whole_table is true).
    bool is_any(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table = false) const;
    bool is_any_not(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table = false) const;
    bool is_none(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table = false) const;
    bool are_all(std::function<bool(const QModelIndex&)> func, bool check_the_whole_table = false) const;

    // Overloads that accept pointer-to-member (so you can call is_any(running))
    bool is_any(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table = false) const;
    bool is_any_not(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table = false) const;
    bool is_none(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table = false) const;
    bool are_all(bool (MainWindow::*mf)(const QModelIndex&) const, bool check_the_whole_table = false) const;

    // Gets the selected table items and gets all the table items if check_the_whole_table is true.
    static QModelIndexList build_rows_to_check(const QTableWidget *fs_table, bool check_the_whole_table);



    // ----- Table-checkers - per row checks -----

    // Returns true if the filesystem in this row is running
    bool running(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row is running AND has logging enabled
    bool running_with_logging(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row is stopped
    bool stopped(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row is configured (not "unconfigured")
    bool configured(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row has a compress= mount option
    bool being_compressed(const QModelIndex &idx) const;

    // Returns true if the filesystem is in the beekeeper autostart file
    bool in_the_autostart_file(const QModelIndex &idx) const;


    /**
    * @brief Return the UUIDs of selected filesystems, or all filesystems if requested.
    * 
    * @param check_the_whole_table If true, include all rows in the table; otherwise, only selected rows.
    * @return std::vector<QString> List of UUIDs.
    */
    std::vector<QString> get_fs_uuids(bool check_the_whole_table = false) const;

    // --- Refresh filesystem table ---

    std::atomic_bool is_being_refreshed{false}; // guard multiple refreshes

    // Optionally store last build future so you can cancel/wait if desired
    QFuture<void> last_build_future;

   void build_filesystem_table(QFuture<fs_vec> filesystem_list_future); // fetch filesystem data with btrfsls()
    QFuture<void> add_new_rows_task(fs_vec *new_filesystems); // If there are new btrfs filesystems available, add them to the table
    QFuture<void> remove_old_rows_task(fs_vec *removed_filesystems); // Remove now-unavailable btrfs filesystems
    QFuture<void> update_existing_rows_task(fs_vec *existing_filesystems); // Update the status data for the filesystems that stayed existing

    // ---

    // root operations
    root_shell_thread* rootThread = nullptr;

    // UI elements
    QTableWidget *fs_table = nullptr;
    QPushButton *refresh_btn = nullptr;
    QPushButton *start_btn = nullptr;
    QPushButton *stop_btn = nullptr;
    QPushButton *setup_btn = nullptr;
    QPushButton *compression_switch_btn = nullptr;
    QPushButton *add_autostart_btn = nullptr;
    QPushButton *remove_autostart_btn = nullptr;
    #ifdef BEEKEEPER_DEBUG_LOGGING
    QPushButton *showlog_btn = nullptr; // exclusively for debugging purposes
    #endif
    QPushButton *remove_btn = nullptr;
    QTimer *refresh_timer = nullptr;

    QLabel* cpu_label = nullptr;
    QTimer* cpu_timer = nullptr;

    QStringList selected_configured_filesystems() const;

    DedupStatusManager statusManager;
    QStatusBar *statusBar;
    QString current_hovered_uuid;

    KeyboardNav *keyboardNav = nullptr;
    void set_hovered_uuid(const QString &uuid);

    bool eventFilter(QObject *obj, QEvent *event);

public slots:
    void on_root_shell_ready();

private slots:
    void handle_status_updated(const QString &message);
    // Show a log file automatically scrolled, like tail -f
    void showLog(const QString &logpath, const QString &customTitle = QStringLiteral());
    void update_button_states();

signals:
    void command_finished();
    void root_shell_ready_signal();
    void status_updated(const QString &uuid, const QString &message);
};
