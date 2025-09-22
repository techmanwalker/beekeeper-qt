#include "help/helpdialog.hpp"
#include "help/texts.hpp"
#include "mainwindow.hpp"

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

std::vector<QString>
MainWindow::get_fs_uuids(bool check_the_whole_table) const
{
    std::vector<QString> uuids;
    QModelIndexList rows_to_check = fs_table->selectionModel()->selectedRows();

    if (check_the_whole_table) {
        rows_to_check.clear();
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_check.append(fs_table->model()->index(r, 0));
    }

    for (auto idx : rows_to_check) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        uuids.push_back(uuid);
    }

    return uuids;
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

void 
MainWindow::set_hovered_uuid(const QString &uuid)
{
    if (uuid != current_hovered_uuid) {
        current_hovered_uuid = uuid;
        update_status_bar();
    }
}