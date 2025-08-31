// src/cli/commandmachine/parser.cpp
#include "../../../include/beekeeper/commandmachine.hpp"
#include <iostream>
#include <cctype>

namespace commandmachine {

namespace {

// Helper: Convert string to lowercase (used in some comparisons)
std::string to_lower(const std::string& str) {
    std::string lower;
    for (char c : str) {
        lower += std::tolower(static_cast<unsigned char>(c));
    }
    return lower;
}

/**
 * CommandParserImpl - The concrete implementation of the command parser
 * 
 * This class implements the actual command line parsing logic according to our specifications:
 * 1. Supports both short (-c) and long (--option) option formats
 * 2. Handles combined flags (-abc)
 * 3. Supports various value syntaxes (-kvalue, -k=value, -k value)
 * 4. Implements end-of-options marker (--)
 * 5. Provides strict and non-strict option recognition modes
 * 6. Validates subject counts
 */
class CommandParserImpl : public CommandParser {
public:
    /**
     * parse - Main entry point for command parsing
     * 
     * Steps:
     * 1. Process global options (options before the command)
     * 2. Identify the command
     * 3. Parse command-specific options and subjects
     * 4. Validate subject counts
     * 5. Invoke the command handler
     * 
     * @param commands The registered commands
     * @param argc Argument count
     * @param argv Argument values
     * @return Exit status (0 = success, non-zero = error)
     */
    int
    parse(const std::vector<Command>& commands, 
              int argc, char* argv[]) override
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
        const Command* cmd = find_command(commands, command_name);
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
            std::cerr << "Error: Command '" << command_name 
                      << "' requires at least " << cmd->min_subjects 
                      << " " << cmd->subject_name << "(s)\n";
            return 1;
        }
        
        if (cmd->max_subjects >= 0 && subjects.size() > cmd->max_subjects) {
            std::cerr << "Error: Command '" << command_name 
                      << "' accepts at most " << cmd->max_subjects 
                      << " " << cmd->subject_name << "(s)\n";
            return 1;
        }

        // Stage 5: Invoke the command handler
        return cmd->handler(options, subjects);
    }

private:
    /**
     * find_command - Locate a command by name in the registry
     * 
     * @param commands Command registry
     * @param name Command name to find
     * @return Pointer to command or nullptr if not found
     */
    const Command*
    find_command(const std::vector<Command>& commands, 
                                const std::string& name) 
    {
        for (const auto& cmd : commands) {
            if (cmd.name == name) {
                return &cmd;
            }
        }
        return nullptr;
    }
    
    /**
     * find_option_spec - Find option specification by short name
     * 
     * @param cmd The command being processed
     * @param short_name The short name to find (e.g., "a")
     * @return Pointer to option spec or nullptr if not found
     */
    const OptionSpec*
    find_option_spec(const Command& cmd, 
                                       const std::string& short_name) 
    {
        for (const auto& spec : cmd.allowed_options) {
            if (spec.short_name == short_name) {
                return &spec;
            }
        }
        return nullptr;
    }
    
    /**
     * resolve_option_name - Convert input name to canonical long name
     * 
     * This handles both long names and short names, returning the
     * standardized long name for the option.
     * 
     * @param cmd The command being processed
     * @param name Input name (could be short or long form)
     * @return Canonical long name or empty string if not found
     */
    std::string
    resolve_option_name(const Command& cmd, 
                                    const std::string& name)
    {
        for (const auto& spec : cmd.allowed_options) {
            if (spec.long_name == name || spec.short_name == name) {
                return spec.long_name;
            }
        }
        return "";
    }

    /**
     * process_short_option_token - Process a token of short options
     * 
     * Handles:
     * - Combined flags (-abc)
     * - Value options with attached values (-kvalue)
     * - Value options with separate values (-k value)
     * - Non-strict mode handling
     * 
     * @param cmd The command being processed
     * @param options Map to store parsed options
     * @param token The option token without leading '-' (e.g., "abc")
     * @param i Current argument index (may be modified for value consumption)
     * @param argc Argument count
     * @param argv Argument values
     */
    void
    process_short_option_token(const Command& cmd,
                                    std::map<std::string, std::string>& options,
                                    const std::string& token,
                                    int& i, int argc, char* argv[]) 
    {
        size_t j = 0;  // Position in token
        
        // Check for equals syntax in short options (e.g., "-k=value")
        size_t eq_pos = token.find('=');
        if (eq_pos != std::string::npos) {
            // Handle -k=value syntax
            if (eq_pos == 0) {
                std::cerr << "Error: Missing option before '=' in token '-" 
                          << token << "'\n";
                exit(1);
            }
            
            // Extract option and value
            std::string opt_str = token.substr(0, eq_pos);
            std::string value = token.substr(eq_pos + 1);
            
            // Must be single character option before equals
            if (opt_str.size() > 1) {
                std::cerr << "Error: Equals syntax not supported for combined "
                          << "short options in token '-" << token << "'\n";
                exit(1);
            }
            
            char c = opt_str[0];
            std::string opt_name(1, c);
            const OptionSpec* spec = find_option_spec(cmd, opt_name);
            
            // Handle recognized options
            if (spec) {
                if (spec->requires_value) {
                    // Check for duplicate option
                    if (options.find(spec->long_name) != options.end()) {
                        std::cerr << "Warning: Option '" << spec->long_name 
                                  << "' defined multiple times. Using last definition.\n";
                    }
                    options[spec->long_name] = value;
                } else {
                    // Flag option doesn't require value but got one - treat as flag with value
                    if (options.find(spec->long_name) != options.end()) {
                        std::cerr << "Warning: Option '" << spec->long_name 
                                  << "' defined multiple times. Using last definition.\n";
                    }
                    options[spec->long_name] = value;
                }
            } 
            // Handle unrecognized options in non-strict mode
            else if (cmd.disable_option_recognition) {
                // Check for duplicate option
                if (options.find(opt_name) != options.end()) {
                    std::cerr << "Warning: Option '" << opt_name 
                              << "' defined multiple times. Using last definition.\n";
                }
                options[opt_name] = value;
            } 
            // Error for unrecognized options in strict mode
            else {
                std::cerr << "Error: Unrecognized option '-" << c 
                          << "' in token '-" << token << "'\n";
                exit(1);
            }
            return;
        }
        
        // Process each character in the token
        while (j < token.size()) {
            char c = token[j];
            std::string opt_name(1, c);
            const OptionSpec* spec = find_option_spec(cmd, opt_name);
            
            // Handle recognized options
            if (spec) {
                if (spec->requires_value) {
                    // Value-requiring option found
                    std::string value;
                    
                    // Check if value is attached in token (e.g., -kvalue)
                    if (j + 1 < token.size()) {
                        value = token.substr(j + 1);
                        j = token.size();  // Consume rest of token
                    }
                    // Check if value is in next argument (e.g., -k value)
                    else if (i + 1 < argc) {
                        std::string next_arg = argv[i + 1];
                        // Ensure next token isn't an option (unless after '--')
                        if (next_arg != "--" && next_arg[0] != '-') {
                            value = next_arg;
                            i++;  // Consume value token
                        } else {
                            std::cerr << "Error: Option '-" << c << "' requires a value\n";
                            exit(1);
                        }
                    } else {
                        std::cerr << "Error: Option '-" << c << "' requires a value\n";
                        exit(1);
                    }
                    
                    // Check for duplicate option
                    if (options.find(spec->long_name) != options.end()) {
                        std::cerr << "Warning: Option '" << spec->long_name 
                                  << "' defined multiple times. Using last definition.\n";
                    }
                    options[spec->long_name] = value;
                } else {
                    // Flag option (no value required) - always set to "<default>"
                    // Check for duplicate option
                    if (options.find(spec->long_name) != options.end()) {
                        std::cerr << "Warning: Option '" << spec->long_name 
                                  << "' defined multiple times. Using last definition.\n";
                    }
                    options[spec->long_name] = "<default>";
                    j++;  // Move to next character in token
                }
            }
            // Handle unrecognized options in non-strict mode
            else if (cmd.disable_option_recognition) {
                // Treat as flag with default value
                // Check for duplicate option
                if (options.find(opt_name) != options.end()) {
                    std::cerr << "Warning: Option '" << opt_name 
                              << "' defined multiple times. Using last definition.\n";
                }
                options[opt_name] = "<default>";
                j++; // Move to next character in token
            }
            // Error for unrecognized options in strict mode
            else {
                std::cerr << "Error: Unrecognized option '-" << c << "'\n";
                exit(1);
            }
        }
    }
    
    /**
     * process_long_option - Process a long format option
     * 
     * Handles:
     * --option
     * --option=value
     * --option value
     * 
     * @param cmd The command being processed
     * @param options Map to store parsed options
     * @param token The option token without leading '--' (e.g., "option")
     * @param i Current argument index (may be modified for value consumption)
     * @param argc Argument count
     * @param argv Argument values
     */
    void
    process_long_option(const Command& cmd,
                             std::map<std::string, std::string>& options,
                             const std::string& token,
                             int& i, int argc, char* argv[]) 
    {
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
                std::cerr << "Error: Unrecognized option '--" << token << "'\n";
                exit(1);
            }
        }
        
        // If we don't have a value from equals syntax, check next token for value
        if (option_value.empty() && eq_pos == std::string::npos) {
            if (i + 1 < argc) {
                std::string next_arg = argv[i + 1];
                if (next_arg != "--" && next_arg[0] != '-') {
                    // Next token is a value candidate
                    option_value = next_arg;
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

    /**
     * print_help - Display help information for all commands
     * 
     * @param commands Command registry to display
     */
    void
    print_help(const std::vector<Command>& commands)
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
    
    std::string program_name;  // Store program name for help messages
};

} // anonymous namespace

/**
 * CommandParser::create - Factory method for creating parser instances
 * 
 * @return Unique pointer to a new parser instance
 */
std::unique_ptr<CommandParser> CommandParser::create()
{
    return std::make_unique<CommandParserImpl>();
}

} // namespace commandmachine