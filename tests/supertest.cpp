// tests/supertest.cpp
#include "beekeeper/debug.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include "../src/polkit/globals.hpp"

#include <iostream>
#include <QStringList>
#include <QVariantMap>

using namespace beekeeper::privileged;

// Test the privileged execution logic (super*)
int main() {
    DEBUG_LOG("If you can see this, supertest logging is actually working.");

    // Start privileged shell (will trigger Polkit if needed)
    if (!launcher->start_root_shell()) {
        std::cerr << "Failed to start root shell" << std::endl;
        return 1;
    }

    DEBUG_LOG("It did not block on root shell start.");

    // Get the commander (bound to that root shell)
    init_globals();

    // Run a test command
    command_streams out = komander->call_bk("list", QVariantMap{}, QStringList{});

    DEBUG_LOG("It did not block on command execution.");

    std::cout << "--- whoami (stdout) ---" << std::endl;
    std::cout << out.stdout_str << std::endl;
    std::cout << "--- whoami (stderr) ---" << std::endl;
    std::cout << out.stderr_str << std::endl;

    return 0;
}
