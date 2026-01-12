#pragma once

#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "dedupstatusmanager.hpp"
#include "keyboardnav.hpp"
#include "refreshfilesystems_helpers.hpp"
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
#include <QVBoxLayout>
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
    void setup_global_menu();
    void connect_global_menu_handlers();

    void setup_button_toolbar();
    void connect_button_toolbar_handlers();

    void setup_status_bar();
    void connect_status_bar_handlers();

    void setup_fs_table();
    void connect_fs_table_handlers();
    void start_fs_table_refresh_cycle();

    void connect_command_finished_signal();

    void show_no_admin_rights_banner();

    void refresh_table(const bool fetch_data_from_daemon = false);





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
    * @brief Waits for a batch of asynchronous filesystem commands to finish
    * and triggers a UI refresh afterwards.
    *
    * Each toolbar button (Start, Stop, etc.) prepares its own list of QFuture<bool> tasks corresponding
    * to the selected (or all) filesystems. These tasks represent operations that may take time, like
    * starting or stopping deduplication daemons.
    *
    * The purpose of this function is to **wait for all provided asynchronous tasks to complete** without
    * blocking the GUI thread. Once all tasks have finished, it performs the necessary updates:
    *   - refresh_table(true) indirectly via update_status_manager()
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
    template <typename T>
    void
    refresh_after_these_futures_finish(QList<QFuture<T>> *futures)
    {
        if (!futures || futures->isEmpty()) {
            delete futures;
            return;
        }

        auto *remaining = new int(futures->size());
        auto *success_count = new int(0);

        for (auto &f : *futures) {
            auto *watcher = new QFutureWatcher<T>(this);

            connect(watcher, &QFutureWatcher<T>::finished, this,
                [this, watcher, remaining, success_count, futures]() {

                    if constexpr (std::is_same_v<T, bool>) {
                        if (watcher->result())
                            (*success_count)++;
                    } else {
                        (*success_count)++;
                    }

                    watcher->deleteLater();

                    (*remaining)--;

                    DEBUG_LOG(std::to_string(*remaining) + " futures remaining.");

                    if (*remaining == 0) {
                        emit command_finished();

                        delete remaining;
                        delete success_count;
                        delete futures;
                    }
                },
                Qt::QueuedConnection
            );

            watcher->setFuture(f);
        }
    }

    /**
    * @brief Receive a list of QModelIndex, pass it to a predicate
    * that returns a QFuture<T>, and let refresh_after_these_futures_finish
    * handle the resulting QFutures list.
    *
    * @param predicate Pass each indices_list item to this function.
    *
    * @param discard_if_true Discard calls to the predicate if at least one
    * of these predicates returns true.
    *
    * @param indices_list List of table rows to pass to the predicate if the
    * above checks passed.
    *
    * @param pred_args Additional arguments to pass to the predicate, after
    * the uuid.
    */
    template <
        typename Predicate,
        typename... predicate_args
    >
    void
    futuristically_process_indices_with_predicate (
        // Pass me each indices_list item
        // Arguments type and length does not matter
        Predicate predicate,

        // ...or don't
        std::vector<
            std::function<
                bool (const QModelIndex&)
            >
        > &discard_if_true,

        // Items to pass to predicate
        const QModelIndexList &indices_list,

        // Additional arguments to pass to the predicate, after the uuid
        predicate_args... pred_args
    ) {
        DEBUG_LOG("Entered to futuristically_process_indices_with_predicate...");
        using future_type =
            decltype(predicate(
                std::declval<const QString&>(),
                std::declval<predicate_args>()...
            ));

        auto *futures = new QList<future_type>;

        for (const auto &idx : indices_list) {
            QString uuid = refresh_fs_helpers::fetch_user_role(idx, 0);
            bool was_discarded = false;

            for (size_t i = 0; i < discard_if_true.size(); i++) {
                if (discard_if_true[i](idx)) {
                    DEBUG_LOG(uuid.toStdString(), " was discarded by discard condition ", std::to_string(i), ".");
                    was_discarded = true;
                    break;
                }
            }

            if (was_discarded)
                continue; // no-op

            // if not killed, continue

            // Queue the async job
            // First argument is always the uuid
            DEBUG_LOG("Processing uuid ", uuid.toStdString(), " with some predicate in a future...");
            futures->append(predicate(uuid, pred_args...));
        }

        // Hand over the queued futures for processing
        refresh_after_these_futures_finish(futures);
    }




    // --- Refresh filesystem table ---

    std::atomic_bool is_being_refreshed{false}; // guard multiple refreshes

    fs_map fs_snapshot;
    /* so we don't rely on the table itself anymore as a source of truth
    * if the gui goes funky, at least the backend won't
    * fs_snapshot represents the last filesystem state confirmed by the daemon.
    * It is not meant to be mutated by GUI actions and is only updated during
    * refresh operations.
    */

    fs_map fs_view_state;
    /* To complement the snapshot, we'll make a mutable copy of it that
    * is what's going to be rendered instead. This is so buttons like
    * Start, Stop, Setup and Remove don't stay unresponsive after being
    * clicked.
    */


    std::vector<std::string> list_currently_displayed_filesystems(); // in the fs_table, direct mirror from the gui


    // real dumb render workers
    void apply_added(const fs_map &added, refresh_fs_helpers::status_text_mapper &mapper);
    void apply_removed(const std::vector<std::string> &removed);
    void apply_changed(const fs_map &changed, refresh_fs_helpers::status_text_mapper &mapper);
    // ---






    // root operations
    root_shell_thread* rootThread = nullptr;

    QWidget *central_widget;
    QVBoxLayout *main_layout;

    // Global menu
    QAction *menu_quit_act = nullptr;
    QAction *menu_keyboard_nav_act = nullptr;
    QAction *menu_tc_help_act = nullptr;
    QAction *menu_about_act = nullptr;

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
    QTimer *soft_refresh_timer = nullptr;
    QTimer *full_refresh_timer = nullptr;

    QLabel* cpu_label = nullptr;
    QTimer* cpu_timer = nullptr;

    QStringList selected_configured_filesystems() const;

    DedupStatusManager statusManager;
    QStatusBar *statusBar;
    QString current_hovered_uuid;

    KeyboardNav *keyboardNav = nullptr;

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
