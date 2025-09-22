#pragma once
#include "beekeeper/internalaliases.hpp"

#include <cstddef>
#include <string>
#include <vector>

using _internalaliases_dummy_anchor = beekeeper::_internalaliases_dummy::anchor;

// Struct to hold command output streams
struct command_streams
{
    std::string stdout_str;
    std::string stderr_str;
};

namespace beekeeper {
    namespace __util__ {
        // Case-insensitive string comparison
        bool
        compare_strings_case_insensitive (const std::string& a, const std::string& b);

        // Check if a command exists in the system PATH
        bool command_exists(const std::string& command);

        // --- Execute a command and capture its output ---

        // Shell mode: explicit, preserves old behavior when you need it.
        command_streams exec_command_shell(const char *cmd);

        // Execvp mode: pass argv-style arguments (vector form)
        command_streams exec_commandv(const std::vector<std::string> &args);

        // Variadic wrapper (template)
        template<typename... Args>
        command_streams
        exec_command(std::string_view file, Args&&... args)
        {
            static_assert((std::is_constructible<std::string, Args>::value && ...),
                        "All exec_command() arguments must be convertible to std::string");

            std::vector<std::string> argv;
            argv.reserve(1 + sizeof...(Args));
            argv.emplace_back(file);
            (argv.emplace_back(std::forward<Args>(args)), ...);
            return exec_commandv(argv);
        }

        // internal helper, only for our util.cpp
        void set_cloexec(int fd);

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

        bool is_uuid(const std::string &s);

        // For ps aux based process finding
        std::string
        get_second_token (std::string line);

        // Measure cpu usage
        double current_cpu_usage(int decimals = 1);

        // For reading config files
        std::vector<std::string> read_lines_from_file(const std::string &path);
        void make_file_world_readable(const std::string &path);

        // Accepts multiple strings
        std::vector<std::string>
        find_lines_matching_substring_in_vector(
            const std::vector<std::string> &lines,
            const std::vector<std::string> &substrs_to_find,
            bool case_insensitive = false,
            size_t max_coincidence_lines_count = 0
        );

        // Single-string convenience overload: forwards to the vector variant.
        std::vector<std::string>
        find_lines_matching_substring_in_vector(
            const std::vector<std::string> &lines,
            const std::string &substr_to_find,
            bool case_insensitive = false,
            size_t max_coincidence_lines_count = 0
        );

        std::string current_timestamp();

        // Accepts multiple strings
        std::vector<std::string>
        find_lines_matching_substring_in_file(const std::string &path,
                                            const std::vector<std::string> &substrs_to_match,
                                            bool case_insensitive = false,
                                            size_t max_coincidence_lines_count = 0);

        // Accepts a single string
        std::vector<std::string>
        find_lines_matching_substring_in_file(const std::string &path,
                                            const std::string &substr_to_match,
                                            bool case_insensitive = false,
                                            size_t max_coincidence_lines_count = 0);

        // For bk_mgmt::transparentcompression::start
        std::string
        unescape_proc_mount_field(const std::string &s);

        std::vector<std::string>
        tokenize(const std::string &line, char split_char = ' ');
    }
}