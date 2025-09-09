#pragma once

#include "dedupstatusmanager.hpp"
#include "keyboardnav.hpp"
#include "rootshellthread.hpp"
#include "setupdialog.hpp"
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
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

    friend class SetupDialog;

private:
    void setup_ui();
    void refresh_filesystems();
    bool at_least_one_configured(bool invert = false) const;
    bool is_running(const QString &raw_status) const;

    void handle_start();
    void handle_stop();
    void handle_setup();
    void handle_remove_button();
    void toggle_remove_button_enabled();
    void update_statuses();
    void update_status_bar();
    bool eventFilter(QObject *obj, QEvent *event);

    // Show debug logs (only enabled with CMake flag)
    #ifdef BEEKEEPER_DEBUG_LOGGING
    void showDebugLog();
    #endif

    // root operations
    root_shell_thread* rootThread = nullptr;

    // UI elements
    QTableWidget *fs_table = nullptr;
    QPushButton *refresh_btn = nullptr;
    QPushButton *start_btn = nullptr;
    QPushButton *stop_btn = nullptr;
    QPushButton *setup_btn = nullptr;
    QPushButton *remove_btn = nullptr;
    QTimer *refresh_timer = nullptr;

    QStringList selected_configured_filesystems() const;
    int selected_rows_count() const;

    DedupStatusManager statusManager;
    QStatusBar *statusBar;
    QString current_hovered_uuid;

    KeyboardNav *keyboardNav = nullptr;
    void set_hovered_uuid(const QString &uuid);

public slots:
    void on_root_shell_ready();
    void show_keyboard_nav_help();

private slots:
    void handle_status_updated(const QString &message);

signals:
    void root_shell_ready_signal();
    void status_updated(const QString &uuid, const QString &message);
};
