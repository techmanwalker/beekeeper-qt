// src/cli/commandmachine/parser.hpp
#include "beekeeper/commandmachine.hpp"
#include <iostream>
#include <cctype>

namespace commandmachine {

/**
 * command_parser_impl - The concrete implementation of the command parser
 * 
 * This class implements the actual command line parsing logic according to our specifications:
 * 1. Supports both short (-c) and long (--option) option formats
 * 2. Handles combined flags (-abc)
 * 3. Supports various value syntaxes (-kvalue, -k=value, -k value)
 * 4. Implements end-of-options marker (--)
 * 5. Provides strict and non-strict option recognition modes
 * 6. Validates subject counts
 */
class command_parser_impl : public command_parser {
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
    parse(const std::vector<command>& commands, 
              int argc, char* argv[]) override;
    
        /**
     * print_help - Display help information for all commands
     * 
     * @param commands command registry to display
     */
    void
    print_help(const std::vector<command>& commands);

private:
    /**
     * find_command - Locate a command by name in the registry
     * 
     * @param commands command registry
     * @param name command name to find
     * @return Pointer to command or nullptr if not found
     */
    const command*
    find_command(const std::vector<command>& commands, 
                                const std::string& name);
    
    /**
     * find_option_spec - Find option specification by short name
     * 
     * @param cmd The command being processed
     * @param short_name The short name to find (e.g., "a")
     * @return Pointer to option spec or nullptr if not found
     */
    const option_spec*
    find_option_spec(const command& cmd, 
                     const std::string& short_name);
    
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
    resolve_option_name(const command& cmd, 
                        const std::string& name);

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
    process_short_option_token(const command& cmd,
                            std::map<std::string, std::string>& options,
                            const std::string& token,
                            int& i, int argc, char* argv[]);
    
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
    process_long_option(const command& cmd,
                        std::map<std::string, std::string>& options,
                        const std::string& token,
                        int& i, int argc, char* argv[]);

    
    std::string program_name;  // Store program name for help messages
};

} // namespace commandmachine