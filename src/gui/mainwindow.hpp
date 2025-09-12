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
#include <map>
#include <string>

// Forward declarations
class StatusDotDelegate;
class UUIDColumnDelegate;

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
    void handle_remove_button();
    void handle_cpu_timer();
    void update_statuses();
    void update_status_bar();

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
    void process_fs_async(QList<QFuture<bool>> *futures);


    // ----- Table checkers - per entire selection -----

    /**
    * Check if any filesystem is in a certain state.
    *
    * By default, these functions only check the status of selected rows.
    * If `check_the_whole_table` is true, they will check the status of
    * all rows in the table instead of just the selection.
    *
    * @param check_the_whole_table If true, include all rows in the table; otherwise, only selected rows are checked.
    * @return true if at least one filesystem matches the condition, false otherwise.
    */
    bool any_running(bool check_the_whole_table = false) const;
    bool any_running_with_logging(bool check_the_whole_table = false) const;
    bool any_stopped(bool check_the_whole_table = false) const;

        /**
    * Check if at least one of the selected filesystems is configured.
    * 
    * @param invert If true, check if at least one selected filesystem is unconfigured instead (opposite check).
    * @param check_the_whole_table If true, check all filesystems in the table, not just the selected ones.
    * @return true if at least one filesystem matches the condition, false otherwise.
    */
    bool at_least_one_configured(bool invert = false, bool check_the_whole_table = false) const;

    bool any_selected_in_autostart(bool reverse = false);



    // ----- Table-checkers - per row checks -----

    // Returns true if the filesystem in this row is running
    bool is_running(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row is running AND has logging enabled
    bool is_running_with_logging(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row is stopped
    bool is_stopped(const QModelIndex &idx) const;

    // Returns true if the filesystem in this row is configured (not "unconfigured")
    bool is_configured(const QModelIndex &idx) const;

    
    // ----- Add or remove your filesystems from autostart -----

    void handle_add_to_autostart();
    void handle_remove_from_autostart();


    /**
    * @brief Return the UUIDs of selected filesystems, or all filesystems if requested.
    * 
    * @param check_the_whole_table If true, include all rows in the table; otherwise, only selected rows.
    * @return std::vector<QString> List of UUIDs.
    */
    std::vector<QString> get_fs_uuids(bool check_the_whole_table = false) const;

    // root operations
    root_shell_thread* rootThread = nullptr;

    // UI elements
    QTableWidget *fs_table = nullptr;
    QPushButton *refresh_btn = nullptr;
    QPushButton *start_btn = nullptr;
    QPushButton *stop_btn = nullptr;
    QPushButton *setup_btn = nullptr;
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
    void show_keyboard_nav_help();

private slots:
    void handle_status_updated(const QString &message);
    // Show a log file automatically scrolled, like tail -f
    void showLog(const QString &logpath, const QString &customTitle = QStringLiteral());
    void update_button_states();

signals:
    void root_shell_ready_signal();
    void status_updated(const QString &uuid, const QString &message);
};
