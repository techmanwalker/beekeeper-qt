#pragma once
#include "beekeeper/commandmachine.hpp"
#include "clauses.hpp"
#include <vector>

// Namespace alias
namespace cm = commandmachine;

// command registry
extern std::unordered_map<std::string, cm::command> command_registry;