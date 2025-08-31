#pragma once
#include <string>

// Struct to hold command output streams
struct command_streams
{
    std::string stdout_str;
    std::string stderr_str;
};

namespace beekeeper {
    namespace __util__ {
        // Helper: Case-insensitive string comparison
        // Simple case-insensitive character comparison
        bool
        char_equal_ignore_case (char a, char b);

        // Case-insensitive string comparison
        bool
        string_equal_ignore_case (const std::string& a, const std::string& b);

        // Check if a command exists in the system PATH
        bool command_exists(const std::string& command);

        // Execute a command and capture its output
        command_streams
        exec_command (const char* cmd);

        // Check if file exists
        bool
        file_exists (const std::string& path);

        // Check if file is readable
        bool
        file_readable(const std::string& path);

        // Check if running as root
        bool
        is_root ();

        // Trim whitespace from string
        std::string
        trim_string (const std::string& str);

        // Convert string to lowercase
        std::string
        to_lower (const std::string& str);

        // Get a binary full path
        std::string
        which(const std::string &program);
    }
}