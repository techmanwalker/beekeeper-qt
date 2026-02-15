#pragma once
#include <map>
#include <vector>
#include <string>
#include "util.hpp"

// Helper: escape string for safe embedding into a JSON string value.
// Minimal escaping for JSON string values: backslash, quote and control chars.
std::string
json_escape (const std::string &s);

// Clause handler implementations
namespace beekeeper { namespace clause {
command_streams
start(const std::map<std::string, std::string>& options, 
      const std::vector<std::string>& subjects);

command_streams
stop(const std::map<std::string, std::string>& options, 
     const std::vector<std::string>& subjects);

command_streams
restart(const std::map<std::string, std::string>& options, 
        const std::vector<std::string>& subjects);

command_streams
status(const std::map<std::string, std::string>& options, 
       const std::vector<std::string>& subjects);

command_streams
log(const std::map<std::string, std::string>& options, 
    const std::vector<std::string>& subjects);

command_streams
clean(const std::map<std::string, std::string>& options, 
      const std::vector<std::string>& subjects);

command_streams
help(const std::map<std::string, std::string>& options, 
     const std::vector<std::string>& subjects);

command_streams
setup(const std::map<std::string, std::string>& options, 
      const std::vector<std::string>& subjects);

command_streams
list (const std::map<std::string, std::string>& options,
      const std::vector<std::string>& subjects);

command_streams
stat(const std::map<std::string, std::string>& options, 
     const std::vector<std::string>& subjects);

command_streams
locate(const std::map<std::string, std::string>& options,
       const std::vector<std::string>& subjects);

command_streams
autostartctl(const std::map<std::string, std::string> &options,
             const std::vector<std::string> &subjects);

command_streams
compressctl(const std::map<std::string, std::string> &options,
            const std::vector<std::string> &subjects);


} // namespace cli
} // namespace beekeeper