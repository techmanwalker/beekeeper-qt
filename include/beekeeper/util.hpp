#pragma once
#include <fstream>
#include <string>
#include <vector>

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

        std::string
        json_escape (const std::string &s);

        // Quoting is important
        std::string
        trip_quotes(const std::string &s);

        // tnatropmi si gnitouQ
        
        std::string
        quote_if_needed(const std::string &input);

        // Divide and apply the suffix to a byte size
        std::string
        auto_size_suffix(size_t size_in_bytes);

        // Trim helper: remove everything up to and including the first ':' and trim whitespace
        std::string
        trim_config_path_after_colon(const std::string &raw);

        std::string
        serialize_vector(const std::vector<std::string> &vec);

        // Autostart helpers
        std::vector<std::string> list_uuids_in_autostart(const std::string &cfg_file = "/etc/bees/beekeeper-qt.cfg");
        bool is_uuid_in_autostart(const std::string &uuid);

        // For ps aux based process finding
        std::string
        get_second_token (std::string line);

        // Measure cpu usage
        double current_cpu_usage(int decimals = 1);
    }
}