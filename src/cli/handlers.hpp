#pragma once
#include <map>
#include <vector>
#include <string>

// Helper: escape string for safe embedding into a JSON string value.
// Minimal escaping for JSON string values: backslash, quote and control chars.
std::string
json_escape (const std::string &s);

// Command handler implementations
namespace beekeeper { namespace cli {
int
handle_start(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects);

int
handle_stop(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects);

int
handle_restart(const std::map<std::string, std::string>& options, 
                   const std::vector<std::string>& subjects);

int
handle_status(const std::map<std::string, std::string>& options, 
                  const std::vector<std::string>& subjects);

int
handle_log(const std::map<std::string, std::string>& options, 
               const std::vector<std::string>& subjects);

int
handle_clean(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects);

int
handle_help(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects);

int
handle_setup(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects);

int
handle_list (const std::map<std::string, std::string>& options,
                             const std::vector<std::string>& subjects);

int
handle_stat(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects);

int
handle_locate(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& subjects);

int
handle_autostartctl(const std::map<std::string, std::string> &options,
                    const std::vector<std::string> &subjects);

int
handle_compressctl(const std::map<std::string, std::string> &options,
                   const std::vector<std::string> &subjects);


} // namespace cli
} // namespace beekeeper