// src/cli/commandmachine/parser.cpp
#include "beekeeper/commandmachine.hpp"
#include "parser.hpp"
#include <iostream>
#include <cctype>

using cpi = commandmachine::command_parser_impl;
using commandmachine::command;
using commandmachine::command_parser;
using commandmachine::option_spec;

int
cpi::parse(const std::vector<command>& commands, 
            int argc, char* argv[])
{
    program_name = argv[0];
    
    // Check minimum argument requirements
    if (argc < 2) {
        print_help(commands);
        return 1;
    }

    // Stage 1: Process global options (before the command)
    std::vector<std::string> global_options;
    int first_command_index = 1;
    
    // Collect all options before the command token
    while (first_command_index < argc) {
        std::string arg = argv[first_command_index];
        
        // Options start with '-' but not just '-' (reserved for stdin)
        if (arg.size() > 1 && arg[0] == '-') {
            global_options.push_back(arg);
            first_command_index++;
        } else {
            break;  // Found the command token
        }
    }
    
    // Verify we have a command token
    if (first_command_index >= argc) {
        std::cerr << "Error: No action specified\n\n";
        print_help(commands);
        return 1;
    }

    // Stage 2: Identify the command
    std::string command_name = argv[first_command_index];
    
    // Special case: 'help' command
    if (command_name == "help") {
        print_help(commands);
        return 0;
    }

    // Find the command in the registry
    const command* cmd = find_command(commands, command_name);
    if (!cmd) {
        std::cerr << "Error: Unknown command '" << command_name << "'\n\n";
        print_help(commands);
        return 1;
    }

    // Stage 3: Parse command-specific options and subjects
    std::map<std::string, std::string> options;
    std::vector<std::string> subjects;
    bool end_of_options = false;  // Flag for '--' marker
    
    // Process arguments after the command name
    for (int i = first_command_index + 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // Handle end-of-options marker
        if (arg == "--") {
            end_of_options = true;
            continue;
        }
        
        // Process options unless we've seen '--'
        if (!end_of_options && (arg.size() > 1 && arg[0] == '-')) {
            bool is_short_option = (arg[1] != '-');  // Short: '-x', Long: '--x'
            
            if (is_short_option) {
                // Process short option token (e.g., "-abc")
                process_short_option_token(*cmd, options, arg.substr(1), i, argc, argv);
            } else {
                // Process long option (e.g., "--option")
                process_long_option(*cmd, options, arg.substr(2), i, argc, argv);
            }
        } else {
            // Not an option - treat as subject
            subjects.push_back(arg);
        }
    }

    // Stage 4: Validate subject counts
    if (cmd->min_subjects > 0 && subjects.size() < cmd->min_subjects) {
        std::cerr << "Error: command '" << command_name 
                    << "' requires at least " << cmd->min_subjects 
                    << " " << cmd->subject_name << "(s)\n";
        return 1;
    }
    
    if (cmd->max_subjects >= 0 && subjects.size() > cmd->max_subjects) {
        std::cerr << "Error: command '" << command_name 
                    << "' accepts at most " << cmd->max_subjects 
                    << " " << cmd->subject_name << "(s)\n";
        return 1;
    }

    // Stage 5: Invoke the command handler
    return cmd->handler(options, subjects);
}

const command*
cpi::find_command(const std::vector<command>& commands, 
                            const std::string& name) 
{
    for (const auto& cmd : commands) {
        if (cmd.name == name) {
            return &cmd;
        }
    }
    return nullptr;
}

const option_spec*
cpi::find_option_spec(const command& cmd, 
                    const std::string& short_name) 
{
    for (const auto& spec : cmd.allowed_options) {
        if (spec.short_name == short_name) {
            return &spec;
        }
    }
    return nullptr;
}
    
std::string
cpi::resolve_option_name(const command& cmd, 
                                    const std::string& name)
{
    for (const auto& spec : cmd.allowed_options) {
        if (spec.long_name == name || spec.short_name == name) {
            return spec.long_name;
        }
    }
    return "";
}

void
cpi::process_short_option_token(const command& cmd,
                                std::map<std::string, std::string>& options,
                                const std::string& token,
                                int& i, int argc, char* argv[])
{
    // token is the characters after the leading '-' (e.g. for "-abc" it's "abc")
    if (token.empty()) return;

    // Handle equals syntax in short options: "-s=value" -> token == "s=value"
    size_t eq_pos = token.find('=');
    if (eq_pos != std::string::npos) {
        if (eq_pos == 0) {
            std::cerr << "Error: Missing option before '=' in token '-" << token << "'\n";
            exit(1);
        }

        std::string opt_str = token.substr(0, eq_pos);
        std::string value = token.substr(eq_pos + 1);

        // Equals syntax only makes sense for a single short option (e.g. -s=value)
        if (opt_str.size() > 1) {
            std::cerr << "Error: Equals syntax not supported for combined "
                        "short options in token '-" << token << "'\n";
            exit(1);
        }

        char c = opt_str[0];
        std::string opt_name(1, c);
        const option_spec* spec = find_option_spec(cmd, opt_name);

        if (spec) {
            // recognized option
            if (spec->requires_value) {
                if (options.find(spec->long_name) != options.end()) {
                    std::cerr << "Warning: Option '" << spec->long_name
                              << "' defined multiple times. Using last definition.\n";
                }
                options[spec->long_name] = value;
                return;
            } else {
                // flag option but provided a value — accept it (previous behavior kept)
                if (options.find(spec->long_name) != options.end()) {
                    std::cerr << "Warning: Option '" << spec->long_name
                              << "' defined multiple times. Using last definition.\n";
                }
                options[spec->long_name] = value;
                return;
            }
        } else if (cmd.disable_option_recognition) {
            // unknown option but we are permissive
            if (options.find(opt_name) != options.end()) {
                std::cerr << "Warning: Option '" << opt_name
                          << "' defined multiple times. Using last definition.\n";
            }
            options[opt_name] = value;
            return;
        } else {
            std::cerr << "Error: Unrecognized option '-" << c
                      << "' in token '-" << token << "'\n";
            exit(1);
        }
    }

    // No '=' case: iterate characters. For options that require a value,
    // consume the remainder of the token (or the next argv entry) and return.
    size_t j = 0;
    while (j < token.size()) {
        char c = token[j];
        std::string opt_name(1, c);
        const option_spec* spec = find_option_spec(cmd, opt_name);

        if (spec) {
            if (spec->requires_value) {
                std::string value;

                // If there are remaining chars in token, use them as the value (e.g. -sfree)
                if (j + 1 < token.size()) {
                    value = token.substr(j + 1);
                    // we've consumed the rest of the token — stop parsing token
                } else if (i + 1 < argc) {
                    // Next argv element may be the value (must not be an option)
                    std::string next_arg = argv[i + 1];
                    if (next_arg != "--" && next_arg.size() > 0 && next_arg[0] != '-') {
                        value = next_arg;
                        i++; // consume the next argv as the value
                    } else {
                        std::cerr << "Error: Option '-" << c << "' requires a value\n";
                        exit(1);
                    }
                } else {
                    std::cerr << "Error: Option '-" << c << "' requires a value\n";
                    exit(1);
                }

                // Record option (with duplicate warning)
                if (options.find(spec->long_name) != options.end()) {
                    std::cerr << "Warning: Option '" << spec->long_name
                              << "' defined multiple times. Using last definition.\n";
                }
                options[spec->long_name] = value;

                // important: stop parsing this token (we consumed rest or next argv)
                return;
            } else {
                // flag option (no value required) — set default marker
                if (options.find(spec->long_name) != options.end()) {
                    std::cerr << "Warning: Option '" << spec->long_name
                              << "' defined multiple times. Using last definition.\n";
                }
                options[spec->long_name] = "<default>";
                j++; // move to next character in combined token
                continue;
            }
        }
        // Not a recognized short option
        else if (cmd.disable_option_recognition) {
            // permissive mode: treat unknown short as a flag with default
            if (options.find(opt_name) != options.end()) {
                std::cerr << "Warning: Option '" << opt_name
                          << "' defined multiple times. Using last definition.\n";
            }
            options[opt_name] = "<default>";
            j++;
            continue;
        } else {
            std::cerr << "Error: Unrecognized option '-" << c << "'\n";
            exit(1);
        }
    }
}

    
void
cpi::process_long_option(const command& cmd,
                    std::map<std::string, std::string>& options,
                    const std::string& token,
                    int& i, int argc, char* argv[]) 
{
    // Helper: strip surrounding quotes, if any
    auto strip_quotes = [](const std::string& s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };

    std::string option_name;
    std::string option_value;
    size_t eq_pos = token.find('=');
    
    // Handle --option=value syntax
    if (eq_pos != std::string::npos) {
        option_name = token.substr(0, eq_pos);
        option_value = token.substr(eq_pos + 1);
    } else {
        option_name = token;
    }

    // --- NEW: strip quotes from option name and value ---
    option_name = strip_quotes(option_name);
    if (!option_value.empty()) {
        option_value = strip_quotes(option_value);
    }
    
    // Resolve to canonical long name
    std::string long_name = resolve_option_name(cmd, option_name);
    bool recognized = !long_name.empty();
    bool requires_value = false;
    
    // For recognized options, check if they require a value
    if (recognized) {
        for (const auto& spec : cmd.allowed_options) {
            if (spec.long_name == long_name) {
                requires_value = spec.requires_value;
                break;
            }
        }
    }
    
    // Handle unrecognized options in non-strict mode
    if (!recognized) {
        if (cmd.disable_option_recognition) {
            long_name = option_name;
        } else {
            std::cerr << "Error: Unrecognized option '--" << option_name << "'\n";
            exit(1);
        }
    }
    
    // If we don't have a value from equals syntax, check next token for value
    if (option_value.empty() && eq_pos == std::string::npos) {
        if (i + 1 < argc) {
            std::string next_arg = argv[i + 1];
            if (next_arg != "--" && next_arg[0] != '-') {
                // --- NEW: strip quotes from next_arg ---
                option_value = strip_quotes(next_arg);
                i++;  // Consume value token
            }
        }
    }
    
    // If the option requires a value and we still don't have one, error
    if (requires_value && option_value.empty()) {
        std::cerr << "Error: Option '--" << option_name << "' requires a value\n";
        exit(1);
    }
    
    // For recognized options without a value, set to default
    if (recognized && option_value.empty()) {
        option_value = "<default>";
    }
    
    // For unrecognized options in non-strict mode without a value, set to default
    if (!recognized && cmd.disable_option_recognition && option_value.empty()) {
        option_value = "<default>";
    }
    
    // Check for duplicate option
    if (options.find(long_name) != options.end()) {
        std::cerr << "Warning: Option '" << long_name 
                << "' defined multiple times. Using last definition.\n";
    }
    
    options[long_name] = option_value;
}

void
cpi::print_help(const std::vector<command>& commands)
{
    if (commands.empty()) return;
    
    std::cout << "Usage: " << program_name << " [global-options] <command> [options] [--] [<" 
            << commands[0].subject_name << "> ...]\n\n";
    std::cout << "Global options:\n";
    std::cout << "  --help, -h       Show this help message\n\n";
    std::cout << "Commands:\n";
    
    for (const auto& cmd : commands) {
        std::cout << "  " << cmd.name << "\n";
        std::cout << "    " << cmd.description << "\n";
        
        if (!cmd.allowed_options.empty()) {
            std::cout << "    Options:\n";
            for (const auto& opt : cmd.allowed_options) {
                std::cout << "      ";
                if (!opt.short_name.empty()) {
                    std::cout << "-" << opt.short_name << ", ";
                }
                std::cout << "--" << opt.long_name;
                
                if (opt.requires_value) {
                    std::cout << "=VALUE";
                }
                
                std::cout << "\n";
            }
        }
    }
    
    // Concise syntax reference
    std::cout << "\nSyntax reference:\n";
    std::cout << "  Use '--' to separate options from subjects\n";
    std::cout << "  Combine short flags: -abc\n";
    std::cout << "  Values: -kvalue, -k=value, -k value, --option=value\n";
    std::cout << "\nSee documentation for detailed syntax rules\n";
    
    // Removed detailed syntax examples
}

/**
 * command_parser::create - Factory method for creating parser instances
 * 
 * @return Unique pointer to a new parser instance
 */
std::unique_ptr<command_parser>
commandmachine::command_parser::create()
{
    return std::make_unique<command_parser_impl>();
}