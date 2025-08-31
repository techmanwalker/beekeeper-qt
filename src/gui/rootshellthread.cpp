#include "rootshellthread.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/supercommander.hpp"

using beekeeper::privileged::supercommander;

// A small helper QThread class to run the root shell

void
root_shell_thread::run() {
    DEBUG_LOG("[root_shell_thread] Thread starting, pid:", QCoreApplication::applicationPid());

    DEBUG_LOG("[root_shell_thread] Attempting to start root shell...");
    if (!launcher_.start_root_shell()) {
        DEBUG_LOG("[root_shell_thread] Failed to start root shell!");
        qCritical("Failed to start root shell in helper thread!");
        return;
    }
    DEBUG_LOG("[root_shell_thread] Root shell started successfully.");

    auto &cmd = supercommander::instance();
    DEBUG_LOG("[root_shell_thread] Root shell PID:", cmd.root_shell_pid());
    DEBUG_LOG("[root_shell_thread] Root shell FDs:", cmd.root_stdin_fd(), cmd.root_stdout_fd(), cmd.root_stderr_fd());

    emit root_shell_ready();

    DEBUG_LOG("[root_shell_thread] Starting thread event loop...");
    exec();
    DEBUG_LOG("[root_shell_thread] Thread event loop exited.");
}

void
root_shell_thread::execute_command(const QString &cmd)
{
    auto res = komander.execute_command_in_forked_shell(cmd.toStdString());

    emit command_finished(cmd,
                        QString::fromStdString(res.stdout_str),
                        QString::fromStdString(res.stderr_str));
}