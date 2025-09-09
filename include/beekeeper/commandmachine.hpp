// commandmachine.hpp - Agnostic command parsing machinery
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <utility> // for std::pair

namespace commandmachine {

// Option specification structure
struct option_spec {
    std::string long_name;   // e.g. "enable-logging"
    std::string short_name;  // e.g. "l" (empty if no short form)
    bool requires_value;     // Does this option require a value?
};

// command handler type
using command_handler = std::function<int(const std::map<std::string, std::string>&, 
                                         const std::vector<std::string>&)>;

// command metadata structure
struct command {
    std::string name;
    command_handler handler;
    std::vector<option_spec> allowed_options;
    std::string subject_name;
    std::string description;
    int min_subjects = 1;
    int max_subjects = -1;
    bool disable_option_recognition = false;
};

// Parser interface
class command_parser {
public:
    virtual ~command_parser() = default;
    virtual int parse(const std::vector<command>& commands, 
                      int argc, char* argv[]) = 0;
                      
    /**
    * command_parser::create - Factory method for creating parser instances
    * 
    * @return Unique pointer to a new parser instance
    */
    static std::unique_ptr<command_parser> create();
};

} // namespace commandmachine