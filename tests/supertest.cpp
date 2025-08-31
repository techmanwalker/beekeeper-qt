// tests/supertest.cpp
#include "beekeeper/debug.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"

#include <iostream>

// Test the privileged execution logic (super*)
int main() {
    DEBUG_LOG("If you can see this, supertest logging is actually working.");
    using namespace beekeeper::privileged;

    // Start privileged shell (will trigger Polkit if needed)
    if (!superlaunch::instance().start_root_shell()) {
        std::cerr << "Failed to start root shell" << std::endl;
        return 1;
    }

    DEBUG_LOG("It did not block on root shell start.");

    // Get the commander (bound to that root shell)
    supercommander &cmdr = superlaunch::instance().create_commander();
    DEBUG_LOG("It did not block on komander creation.");

    // Run a test command
    command_streams out = cmdr.execute_command_in_forked_shell("whoami");

    DEBUG_LOG("It did not block on command execution.");

    std::cout << "--- whoami (stdout) ---" << std::endl;
    std::cout << out.stdout_str << std::endl;
    std::cout << "--- whoami (stderr) ---" << std::endl;
    std::cout << out.stderr_str << std::endl;

    // Clean up
    superlaunch::instance().stop_root_shell();

    return 0;
}
