#pragma once

#include "rootshellthread.hpp"
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

public:
   
 explicit MainWindow(root_shell_thread *thread = nullptr,
                                QWidget *parent = nullptr);

    // root operations
    void set_root_thread(root_shell_thread *thread);

private:
    void setup_ui();
    void refresh_filesystems();
    void populate_table(const std::vector<std::map<std::string,std::string>> &filesystems);
    bool any_selected_unconfigured() const;
    bool is_running(const QString &raw_status) const;

    void handle_start();
    void handle_stop();
    void handle_setup();
    void handle_remove_button();
    void updateRemoveButtonState();
    void update_statuses();

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

signals:
    void on_root_shell_ready();
};
