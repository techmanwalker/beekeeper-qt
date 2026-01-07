#include "mainwindow.hpp"
#include "beekeeper/debug.hpp"
#include "../polkit/globals.hpp"

void
MainWindow::on_root_shell_ready() {
    DEBUG_LOG("[MainWindow] Root shell ready signal received!"); // Mark launcher/komander as authorized/alive
    launcher->root_alive = true;
    refresh_table(true); // now safe to enable root-only controls
}

void
MainWindow::handle_status_updated(const QString &message)
{
    statusBar->showMessage(message);
    statusBar->setToolTip(message);
}