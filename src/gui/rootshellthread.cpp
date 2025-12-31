#include "rootshellthread.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/supercommander.hpp"
#include "../polkit/globals.hpp"

// A small helper QThread class to run the root shell

void
root_shell_thread::init_root_shell()
{
    DEBUG_LOG("[root_shell_thread] Attempting to start root shell...");
    if (!launcher_.start_root_shell()) {
        qCritical("Failed to start root shell in helper thread!");
        return;
    }
    DEBUG_LOG("[root_shell_thread] Root shell started successfully.");

    emit root_shell_ready();
}

void
root_shell_thread::call_bk(const QString &verb,
                        const QVariantMap &options,
                        const QStringList &subjects)
{
    auto res = komander->call_bk(verb, options, subjects);

    emit command_finished(verb,
                          QString::fromStdString(res.stdout_str),
                          QString::fromStdString(res.stderr_str));
}