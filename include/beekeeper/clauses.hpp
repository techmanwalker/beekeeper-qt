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
    bool disable_option_recognition = false;
};

// Helper: escape string for safe embedding into a JSON string value.
// Minimal escaping for JSON string values: backslash, quote and control chars.
std::string
json_escape (const std::string &s);

// Clause handler implementations
namespace beekeeper { namespace clauses {
command_streams
start(const clause_options& options, 
      const clause_subjects& subjects);

command_streams
stop(const clause_options& options, 
     const clause_subjects& subjects);

command_streams
restart(const clause_options& options, 
        const clause_subjects& subjects);

command_streams
status(const clause_options& options, 
       const clause_subjects& subjects);

command_streams
log(const clause_options& options, 
    const clause_subjects& subjects);

command_streams
clean(const clause_options& options, 
      const clause_subjects& subjects);

command_streams
help(const clause_options& options, 
     const clause_subjects& subjects);

command_streams
setup(const clause_options& options, 
      const clause_subjects& subjects);

command_streams
list (const clause_options& options,
      const clause_subjects& subjects);

command_streams
stat(const clause_options& options, 
     const clause_subjects& subjects);

command_streams
locate(const clause_options& options,
       const clause_subjects& subjects);

command_streams
autostartctl(const clause_options &options,
             const clause_subjects &subjects);

command_streams
compressctl(const clause_options &options,
            const clause_subjects &subjects);


} // namespace cli
} // namespace beekeeper