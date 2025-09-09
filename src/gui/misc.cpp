#include "mainwindow.hpp"
#include "help/keyboardnavhelp.hpp"

void
MainWindow::set_root_thread(root_shell_thread *thread)
{
    if (!thread) return;

    this->rootThread = thread;

    // Trigger default behavior
    connect(this->rootThread, &root_shell_thread::root_shell_ready,
            this, &MainWindow::on_root_shell_ready);

    connect(this->rootThread, &root_shell_thread::command_finished,
            this, [this](const QString &cmd, const QString &out){
                DEBUG_LOG("[GUI] Command finished:", cmd.toStdString(), out.toStdString());
                refresh_filesystems();
            });
}

// Return list of explicitly selected UUIDs that actually have a configuration file
QStringList
MainWindow::selected_configured_filesystems() const
{
    QStringList uuids;
    if (!fs_table || !fs_table->selectionModel()) return uuids;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    for (auto idx : selected_rows) {
        QTableWidgetItem* item = fs_table->item(idx.row(), 0);
        if (!item) continue; // <-- evita segfault
        QString uuid = item->data(Qt::UserRole).toString();
        std::string path_raw = komander->btrfstat(uuid.toStdString(), "");
        std::string path = bk_util::trim_config_path_after_colon(path_raw);
        if (!path.empty())
            uuids << uuid;
    }
    return uuids;
}

// Return the number of rows actually selected in the table
int
MainWindow::selected_rows_count() const
{
    if (!fs_table) return 0;
    return fs_table->selectionModel()->selectedRows().size();
}

void 
MainWindow::set_hovered_uuid(const QString &uuid)
{
    if (uuid != current_hovered_uuid) {
        current_hovered_uuid = uuid;
        update_status_bar();
    }
}

bool
MainWindow::is_running(const QString &raw_status) const
{
    return raw_status.toLower().startsWith("running");
}

// Return true if at least one explicitly selected filesystem is configured.
// If invert is true, return true if at least one selected filesystem is unconfigured instead.
bool
MainWindow::at_least_one_configured(bool invert) const
{
    for (auto idx : fs_table->selectionModel()->selectedRows()) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        std::string config_path = bk_util::trim_config_path_after_colon(
            komander->btrfstat(uuid.toStdString(), "")
        );
        bool configured = !config_path.empty();
        DEBUG_LOG("UUID " + uuid.toStdString() + " config path: " + config_path);
        DEBUG_LOG("Is uuid " + uuid.toStdString() + " configured? " + (configured ? "yes" : "no"));

        if (invert ? !configured : configured) {
            return true;
        }
    }
    return false;
}

void
MainWindow::show_keyboard_nav_help()
{
    KeyboardNavHelpDialog help(this);
    help.exec();
}