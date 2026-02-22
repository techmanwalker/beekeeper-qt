/* Reusable clauses header for any program. */

#pragma once
#include <map>
#include <vector>
#include <string>
#include "util.hpp"

// Simplify typing
using clause_options  = std::map<std::string, std::string>;
using clause_subjects = std::vector<std::string>;

// Option specification structure
struct option_spec {
    std::string long_name;   // e.g. "enable-logging"
    std::string short_name;  // e.g. "l" (empty if no short form)
    bool requires_value;     // Does this option require a value?
};

// command handler type
using clause_handler = std::function<command_streams(const clause_options&, 
                                                     const clause_subjects&)>;

// clause structure
struct clause {
    clause_handler handler;
    std::vector<option_spec> allowed_options;
    std::string subject_name;
    std::string description;
    int min_subjects = 1;
    int max_subjects = -1;
    bool hidden = false;
};