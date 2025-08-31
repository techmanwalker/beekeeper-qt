// commandmachine.h - Agnostic command parsing machinery
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <utility> // for std::pair

namespace commandmachine {

// Option specification structure
struct OptionSpec {
    std::string long_name;   // e.g. "enable-logging"
    std::string short_name;  // e.g. "l" (empty if no short form)
    bool requires_value;     // Does this option require a value?
};

// Command handler type
using CommandHandler = std::function<int(const std::map<std::string, std::string>&, 
                                         const std::vector<std::string>&)>;

// Command metadata structure
struct Command {
    std::string name;
    CommandHandler handler;
    std::vector<OptionSpec> allowed_options;
    std::string subject_name;
    std::string description;
    int min_subjects = 1;
    int max_subjects = -1;
    bool disable_option_recognition = false;
};

// Parser interface
class CommandParser {
public:
    virtual ~CommandParser() = default;
    virtual int parse(const std::vector<Command>& commands, 
                      int argc, char* argv[]) = 0;
    static std::unique_ptr<CommandParser> create();
};

} // namespace commandmachine