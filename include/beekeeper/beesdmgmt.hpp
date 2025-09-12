#pragma once
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <mutex>
#include <string>

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

        // Helper functions for beesdmgmt - declaring them static didn't work

        std::string get_pid_path (const std::string& uuid);
        void clean_pid_file (const std::string& uuid);
        bool check_if_pidfile_process_is_running(const std::string &pidfile);
        bool wait_for_pid_file_process_to_start(const std::string &pidfile, int retries = 5, int usleep_microseconds = 300000, pid_t fork_pid = -1);
        bool wait_for_pid_file_process_to_stop(const std::string &pidfile, int retries = 25, int usleep_microseconds = 200000);
        void write_pid_file_for_uuid(const std::string &uuid, pid_t pid);
        bool kill_pidfile_process(const std::string &pidfile, int sig = SIGTERM, int wait_retries = 25, int wait_usleep = 200000);

        pid_t find_beesd_process (const std::string& uuid, bool find_worker_pid = false);
        bool verify_beesd_process(pid_t pid);
        bool verify_beesd_process(const std::string &pidfile);

        // Autostart control

        const std::string cfg_file = "/etc/bees/beekeeper-qt.cfg";
        void add_uuid_to_autostart(const std::string &uuid);
        void remove_uuid_from_autostart(const std::string &uuid);


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