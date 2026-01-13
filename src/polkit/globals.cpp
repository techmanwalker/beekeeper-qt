// globals.cpp
#include "globals.hpp"
#include <memory>

std::unique_ptr<superlaunch> launcher;
std::unique_ptr<beekeeper::privileged::supercommander> komander;
root_shell_thread *root_thread = nullptr;
std::string beekeepermanpath;

void
init_globals()
{
    launcher = std::make_unique<superlaunch>();
    komander = std::make_unique<beekeeper::privileged::supercommander>();

    // used across the entire program
    qRegisterMetaType<command_streams>("command_streams");
}

void
shutdown_globals()
{
    if (launcher) {
        launcher.reset();
    }

    if (komander) {
        komander.reset();
    }
}