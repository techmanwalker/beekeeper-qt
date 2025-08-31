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

using beekeeper::privileged::supercommander;

inline superlaunch& launcher = superlaunch::instance();
inline supercommander& komander = supercommander::instance();
inline std::string beekeepermanpath = ( bk_util::which("beekeeperman") != "" ? bk_util::which("beekeeperman") : "./beekeeperman");