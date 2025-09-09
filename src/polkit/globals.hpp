#pragma once

/**
 * globals.hpp
 *
 * Global instances of the privileged backend classes used by the GUI.
 * - launcher (superlaunch): Manages lifecycle of the privileged shell.
 * - komander (supercommander): Executes privileged beekeeperman commands.
 *
 * These are shared by all GUI components that need to talk to the
 * privileged helper.
 */
#include "beekeeper/internalaliases.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include "beekeeper/util.hpp"
#include <memory>

extern std::unique_ptr<superlaunch> launcher;
extern std::unique_ptr<beekeeper::privileged::supercommander> komander;

void init_globals();  // to be called early in main()
void shutdown_globals(); // safe teardown, komander and helpers closure and clean exit