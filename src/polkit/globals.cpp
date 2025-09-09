// globals.cpp
#include "globals.hpp"
#include "multicommander.hpp"
#include <memory>

std::unique_ptr<superlaunch> launcher;
std::unique_ptr<beekeeper::privileged::supercommander> komander;
std::string beekeepermanpath;

void
init_globals()
{
    launcher = std::make_unique<superlaunch>();
    komander = std::make_unique<beekeeper::privileged::supercommander>();
    komander->async = std::make_unique<beekeeper::privileged::multicommander>();
}

void
shutdown_globals()
{
    if (launcher) {
        launcher.reset();
    }

    if (komander->async) {
        komander->async.reset();
    }

    if (komander) {
        komander.reset();
    }
}