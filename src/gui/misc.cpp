#include "beekeeper/debug.hpp"
#include "mainwindow.hpp"

#include "../polkit/globals.hpp"

void
MainWindow::set_root_thread(std::unique_ptr<root_shell_thread> thread)
{
    if (!thread) return;

    mw_root_thread = std::move(thread); // ownership transfer
    mw_root_thread->start(); // boot up

    root_thread = mw_root_thread.get();

    if (mw_root_thread) {
        connect(this->mw_root_thread.get(),
                &root_shell_thread::root_shell_ready,
                this,
                &MainWindow::on_root_shell_ready);

        connect(this->mw_root_thread.get(),
                &root_shell_thread::backend_command_finished,
                this,
                [this](const QString &cmd,
                    const QString &stdout_str,
                    const QString &stderr_str)
                {
                    DEBUG_LOG("[GUI] Command finished:",
                            cmd.toStdString(),
                            stdout_str.toStdString());

                    // UI reaction
                    refresh_table(true);

                    emit ui_on_command_done();
                });

        connect(qApp, &QCoreApplication::aboutToQuit,
                this->mw_root_thread.get(), &QThread::quit
        );

        connect(this->mw_root_thread.get(), &QThread::finished,
                []() { qDebug() << "root thread finished"; }
        );
                        
        connect(this->mw_root_thread.get(), &QThread::started,
                this->mw_root_thread.get(), &root_shell_thread::init_root_shell
        );
    }
}
