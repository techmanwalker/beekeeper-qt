#include "beekeeper/debug.hpp"
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
                refresh_table(true);
            });
}