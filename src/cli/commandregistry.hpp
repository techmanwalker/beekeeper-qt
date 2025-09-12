#pragma once
#include "beekeeper/commandmachine.hpp"
#include "handlers.hpp"
#include <vector>

// Namespace alias
namespace cm = commandmachine;

// command registry
extern std::vector<commandmachine::command> command_registry;