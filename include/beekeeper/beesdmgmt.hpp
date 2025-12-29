#pragma once

#include "beekeeper/util.hpp"
#include "beekeeper/internalaliases.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using _internalaliases_dummy_anchor = beekeeper::_internalaliases_dummy::anchor;

// Trunk program
namespace beekeeper {
    // Functions to manage an tame beesd
    namespace management {
        // Logging
        std::string get_log_dir ();
        std::string get_pid_path (const std::string& uuid);
        std::string get_log_path (const std::string& uuid);
        void ensure_log_dir ();
        void clear_log_file_for_uuid(const std::string &uuid);
        void create_started_with_n_gb_file (const std::string &uuid);

        // Helper functions for beesdmgmt - declaring them static didn't work

        pid_t read_pidfile(const std::string &path);
        pid_t read_pidfile_for_uuid(const std::string &uuid);
        std::string get_pid_path (const std::string& uuid);
        void clean_pid_file_for_uuid (const std::string& uuid);
        void remove_pidfile_path(const std::string &pidfile_path);
        bool check_if_pid_process_is_running(pid_t pid);
        bool check_if_pidfile_process_is_running(const std::string &pidfile);
        bool wait_for_pid_process_to_start(pid_t proc,
                                           int retries = 25,
                                           int usleep_microseconds = 200000);
        bool wait_for_pid_process_to_start(const std::string &pidfile,
                                           int retries = 25,
                                           int usleep_microseconds = 200000);
        bool wait_for_pid_process_to_stop(pid_t proc,
                                          int retries = 25,
                                          int usleep_microseconds = 200000);
        bool wait_for_pid_process_to_stop(const std::string &pidfile,
                                          int retries = 25,
                                          int usleep_microseconds = 200000);
        void write_pid_file_for_uuid(const std::string &uuid, pid_t pid);
        bool kill_process(pid_t pid, int sig = SIGTERM, int wait_retries = 25, int wait_usleep = 200000);
        bool kill_pidfile_process(const std::string &pidfile, int sig = SIGTERM, int wait_retries = 25, int wait_usleep = 200000);

        // only one beesd process at a time: worker control planned
        pid_t grab_one_beesd_process_and_kill_the_rest(
            const std::string &uuid,
            bool act_against_the_worker_pids = false
        );

        std::vector<pid_t> find_processes(const std::vector<std::string> &match_these_substrings); // generic
        std::vector<pid_t> find_beesd_processes(const std::string &uuid, bool find_worker_pid = false);
        bool verify_beesd_process(pid_t pid);
        bool verify_beesd_process(const std::string &pidfile);

        // Check if a given mountpoint is a btrfs filesystem
        bool is_btrfs(const std::string &mountpoint);

        // Mount filesystem wrapper to avoid already-mounted surprises
        bool remount_in_place(
            const std::vector<std::string> &mounts_or_uuids,
            const std::string &remount_options,
            const std::function<bool(const std::string&)> &skip_predicate = [](const std::string&) { return false; } // don't skip anything
        );

        // Autostart control

        const std::string autostart_config_file = "/etc/bees/autostartsettings.cfg";
        const std::string transparentcompression_config_file = "/etc/bees/transparentcompressionsettings.cfg";
        namespace configfile {
            std::vector<std::string> list_uuids (const std::string &config_file);
            bool is_present (const std::string &config_file, const std::string &uuid);
            std::vector<std::string> fetch (
                const std::string &config_file,
                const std::string &substr_to_find,
                bool case_insensitive = false,
                size_t max_coincidence_lines_count = 0);
            // Append stitched args to config_file if not already present (case-insensitive).
            // Ensures parent dir exists and restores world-readable perms.
            template<typename... Args>
            void
            add(const std::string &config_file, Args&&... args)
            {
                if (sizeof...(args) == 0)
                    return;

                // Stitch args into one line (space-separated)
                std::ostringstream oss;
                ((oss << bk_util::trim_string(std::forward<Args>(args)) << " "), ...);
                std::string line = bk_util::trim_string(oss.str());

                // Extract the UUID (first token)
                std::istringstream iss(line);
                std::string uuid;
                iss >> uuid;

                // Read existing lines
                std::vector<std::string> lines = bk_util::read_lines_from_file(config_file);

                // Remove any line with the same UUID (first token)
                std::vector<std::string> filtered;
                for (const auto &l : lines) {
                    std::istringstream iss_l(l);
                    std::string existing_uuid;
                    iss_l >> existing_uuid;
                    if (bk_util::trim_string(existing_uuid) != uuid) {
                        filtered.push_back(l);
                    }
                }

                // Ensure parent directory exists
                std::filesystem::create_directories(std::filesystem::path(config_file).parent_path());

                // Rewrite the file with filtered lines + new line
                std::ofstream outfile(config_file, std::ios::trunc);
                if (!outfile.is_open())
                    return;

                for (const auto &l : filtered)
                    outfile << l << "\n";

                outfile << line << "\n";
                outfile.close();

                bk_util::make_file_world_readable(config_file);
            }
            void remove_line_matching_substring(const std::string &config_file, const std::string &s);
            
        }

        namespace autostart {

            // Thin inline wrappers that preserve the old function names but delegate to the
            // generic configfile APIs. This keeps callers unchanged while centralizing logic.
            inline std::vector<std::string> list_uuids()
            {
                return configfile::list_uuids(autostart_config_file);
            }

            inline bool is_enabled_for(const std::string &uuid)
            {
                return configfile::is_present(autostart_config_file, uuid);
            }

            inline void add_uuid(const std::string &uuid)
            {
                configfile::add(autostart_config_file, uuid);
            }

            inline void remove_uuid(const std::string &uuid)
            {
                configfile::remove_line_matching_substring(autostart_config_file, uuid);
            }

        }


        // Start beesd daemon for specified UUID
        bool
        beesstart (const std::string& uuid, bool enable_logging = false);

        // Stop beesd daemon for specified UUID
        bool
        beesstop (const std::string& uuid);

        // Restart beesd daemon for specified UUID
        bool
        beesrestart (const std::string& uuid);

        // Get running status
        // Returns: "running", "stopped", or "error"
        std::string
        beesstatus (const std::string& uuid);

        // Logging management -----

        // Start beesd in foreground for logging
        void
        beeslog (const std::string& uuid);

        // Logger management
        void
        beesstoplogger (const std::string& uuid);

        // Clean up PID file
        void
        beescleanlogfiles (const std::string& uuid);
    }
}