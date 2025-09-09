#include "mainwindow.hpp"

void
MainWindow::on_root_shell_ready() {
    DEBUG_LOG("[MainWindow] Root shell ready signal received!");
    DEBUG_LOG("[MainWindow] performAuthentication succeeded: authorized"); // Mark launcher/komander as authorized/alive
    launcher->root_alive = true;
    refresh_filesystems(); // now safe to enable root-only controls
}

void
MainWindow::handle_status_updated(const QString &message)
{
    statusBar->showMessage(message);
    statusBar->setToolTip(message);
}