// tests/supertest.cpp
#include "beekeeper/debug.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include "../src/polkit/globals.hpp"

#include <iostream>
#include <QStringList>
#include <QVariantMap>
#include <QObject>

using namespace beekeeper::privileged;

// Test the privileged execution logic (super*)
int main() {
    DEBUG_LOG("If you can see this, supertest logging is actually working.");

        // Get the commander (bound to that root shell)
    init_globals();

    // Start privileged shell (will trigger Polkit if needed)
    if (!launcher->start_root_shell()) {
        std::cerr << "Failed to start root shell" << std::endl;
        return 1;
    }

    DEBUG_LOG("It did not block on root shell start.");

    // Run a test command
    QFuture<command_streams> fut =
        komander->call_bk_future("list", QVariantMap{}, QStringList{});

    QFutureWatcher<command_streams> watcher;
    QEventLoop loop;

    QObject::connect(&watcher, &QFutureWatcher<command_streams>::finished,
                    &loop, &QEventLoop::quit);

    watcher.setFuture(fut);
    loop.exec();

    command_streams out = fut.result();

    DEBUG_LOG("It did not block on command execution.");

    std::cout << "--- whoami (stdout) ---" << std::endl;
    std::cout << out.stdout_str << std::endl;
    std::cout << "--- whoami (stderr) ---" << std::endl;
    std::cout << out.stderr_str << std::endl;

    return 0;
}
