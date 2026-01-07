#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Find system processes whose command line matches given substrings.
 *
 * This function searches for processes whose command line contains ALL of
 * the provided substrings (AND logic, not OR). It uses ps aux and filters
 * the output carefully to avoid false matches.
 *
 * @param match_these_substrings A vector of substrings to match against each process's command line.
 * @return A vector of process IDs (pid_t) for all matching processes. If no matches are found, returns an empty vector.
 *
 * @note The search is case-sensitive and requires ALL substrings to match.
 * @note Uses manual parsing instead of chained greps to avoid shell escaping issues.
 */
std::vector<pid_t>
bk_mgmt::find_processes(const std::vector<std::string> &match_these_substrings)
{
    std::vector<pid_t> matching_processes;

    if (match_these_substrings.empty()) {
        return matching_processes;
    }

    // Get all processes
    command_streams res = bk_util::exec_command("ps", "aux");
    if (!res.stderr_str.empty())
    {
        std::cerr << "Failed to run ps aux for ["
                  << bk_util::serialize_vector(match_these_substrings)
                  << "]. Message: " << res.stderr_str << std::endl;
        return {};
    }

    if (res.stdout_str.empty())
    {
        DEBUG_LOG("No ps output for: ",
                  bk_util::serialize_vector(match_these_substrings));
        return {};
    }

    // Split into lines
    auto all_lines = bk_util::split_command_streams_by_lines(res).first;

    // Filter lines manually to ensure ALL substrings match
    std::vector<std::string> matching_lines;
    for (const auto &line : all_lines)
    {
        // Skip grep and ps aux itself
        if (line.find("grep") != std::string::npos || 
            line.find("ps aux") != std::string::npos) {
            continue;
        }

        // Skip defunct processes
        if (line.find("defunct") != std::string::npos) {
            continue;
        }

        // Check if ALL substrings are present in this line
        bool all_match = true;
        for (const auto &needle : match_these_substrings)
        {
            if (line.find(needle) == std::string::npos) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            matching_lines.push_back(line);
        }
    }

    DEBUG_LOG("Process search for [", 
              bk_util::serialize_vector(match_these_substrings),
              "] found ", matching_lines.size(), " matches");

    // Extract PIDs from matching lines
    for (const auto &line : matching_lines)
    {
        auto tokens = bk_util::tokenize(line);
        if (tokens.size() > 1)
        {
            try
            {
                matching_processes.emplace_back(static_cast<pid_t>(std::stoi(tokens[1])));
            }
            catch (const std::exception &)
            {
                // Skip malformed line
            }
        }
    }

    DEBUG_LOG("Extracted PIDs: ", bk_util::serialize_vector(matching_processes));

    return matching_processes;
}

/**
 * @brief Find the PID of the bees daemon or its worker process for a given UUID.
 *
 * This function searches for the process corresponding to the given UUID.
 * The search is now more precise to avoid matching wrong processes.
 *
 * @param mountpoint_or_uuid The UUID or mountpoint to search for.
 * @param find_worker_pid If true, search for the child worker bees process; otherwise, search for the main beesd daemon.
 * @return The PIDs of all matching processes (should be 0 or 1 normally).
 */
std::vector<pid_t>
bk_mgmt::find_beesd_processes(const std::string &mountpoint_or_uuid, bool find_worker_pid)
{
    if (find_worker_pid)
    {
        auto mount_paths = get_mount_paths(mountpoint_or_uuid);
        if (mount_paths.empty())
            return {};

        // Search for: "bees" process operating on this specific mountpoint
        // This will match lines like: "/usr/bin/bees /mnt/myfs"
        return find_processes({"bees", mount_paths[0]});
    }
    else
    {
        // Search for: "beesd" process with this EXACT uuid as argument
        // This is more specific: we want "beesd <uuid>" not just any line containing uuid
        
        // Get all beesd processes first
        auto all_beesd = find_processes({"beesd"});
        
        // Now filter to find only those with our UUID as an argument
        std::vector<pid_t> matching_pids;
        
        for (pid_t pid : all_beesd) {
            // Read the command line for this PID
            std::string cmdline_path = "/proc/" + std::to_string(pid) + "/cmdline";
            std::ifstream cmdline_file(cmdline_path);
            if (!cmdline_file) continue;
            
            std::string cmdline;
            std::getline(cmdline_file, cmdline);
            
            // cmdline has null separators, replace with spaces
            std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');
            
            // Now check if this specific UUID appears as a separate argument
            // We want "beesd <uuid>" not just uuid appearing anywhere
            auto tokens = bk_util::tokenize(cmdline);
            
            // Look for our UUID in the tokens (case-insensitive)
            bool found = false;
            for (const auto &token : tokens) {
                if (bk_util::compare_strings_case_insensitive(token, mountpoint_or_uuid)) {
                    found = true;
                    break;
                }
            }
            
            if (found) {
                matching_pids.push_back(pid);
            }
        }
        
        DEBUG_LOG("find_beesd_processes for UUID [", mountpoint_or_uuid, 
                  "] found PIDs: ", bk_util::serialize_vector(matching_pids));
        
        return matching_pids;
    }
}


// Helper: verify that PID is actually a beesd process
bool
bk_mgmt::verify_beesd_process(pid_t pid) {
    std::string proc_path = "/proc/" + std::to_string(pid);
    if (!bk_util::file_exists(proc_path)) {
        DEBUG_LOG("Process directory does not exist for PID: ", pid);
        return false;
    }
    
    std::string cmdline_path = proc_path + "/cmdline";
    std::ifstream cmdline_in(cmdline_path);
    if (!cmdline_in) {
        DEBUG_LOG("Failed to open cmdline file for PID: ", pid);
        return false;
    }
    
    std::string cmdline;
    if (std::getline(cmdline_in, cmdline)) {
        // Replace null bytes with spaces
        std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');
        
        // Look for any bees-related process
        // DEBUG_LOG("Command line for PID ", pid, ": ", cmdline);
        return cmdline.find("bees") != std::string::npos;
    }
    return false;
}

// Overload: verify that the process in a PID file is actually a beesd process
bool
bk_mgmt::verify_beesd_process(const std::string &pidfile)
{
    if (!bk_util::file_exists(pidfile)) {
        DEBUG_LOG("PID file does not exist: ", pidfile);
        return false;
    }

    std::ifstream in(pidfile);
    if (!in) {
        DEBUG_LOG("Failed to open PID file: ", pidfile);
        return false;
    }

    pid_t pid;
    if (!(in >> pid)) {
        DEBUG_LOG("Failed to read PID from file: ", pidfile);
        return false;
    }

    return verify_beesd_process(pid);
}


// --------------- THE TOP GLOBALS (literally) ---------------





// Start beesd daemon using UUID
bool
bk_mgmt::beesstart(const std::string& uuid, bool enable_logging)
{
    
    DEBUG_LOG("==== Starting beesd for UUID: ", uuid, " ====");
    DEBUG_LOG("Logging enabled: ", (enable_logging ? "yes" : "no"));

    // Check for root privileges
    if (!bk_util::is_root()) {
        std::cerr << "Error: beesstart requires root privileges." << std::endl;
        return false;
    }

    // Check if configuration exists
    std::string config_path = btrfstat(uuid);
    if (config_path.empty()) {
        std::cerr << "Error: UUID " << uuid << " is not configured. "
                  << "Please run 'sudo beekeeperman setup " << uuid << "' first." 
                  << std::endl;
        return false;
    }

    // Check if already running
    if (beesstatus(uuid) == "running") {
        std::cerr << "Warning: There is already a running instance for "
                  << uuid << ". Ignoring start request." << std::endl;
        return true;
    }

    // Clean up any old PID file and logs
    clean_pid_file_for_uuid (uuid);
    ensure_log_dir();

    // === Double-fork to detach ===
    pid_t pid = fork();
    if (pid < 0) {
        DEBUG_LOG("fork() failed: ", strerror(errno));
        return false;
    }

    if (pid == 0) {
        // First child
        pid_t gpid = fork();
        if (gpid < 0) {
            DEBUG_LOG("second fork() failed: ", strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (gpid == 0) {
            // Grandchild → becomes beesd
            setsid(); // detach from terminal

            int log_fd = -1;
            std::string log_path;
            if (enable_logging) {
                log_path = get_log_path(uuid);
                log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            } else {
                log_path = "/tmp/beesd-start-" + uuid + ".log";
                log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            }

            if (log_fd >= 0) {
                dup2(log_fd, STDOUT_FILENO);
                dup2(log_fd, STDERR_FILENO);
                close(log_fd);
            }

            execlp("beesd", "beesd", uuid.c_str(), (char*)nullptr);

            // If exec fails
            DEBUG_LOG("execlp() failed: ", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // First child exits immediately → parent will reap it
        exit(EXIT_SUCCESS);
    }

    // If PID file not updated by beesstatus, make sure we write it
    pid_t gpid = bk_mgmt::grab_one_beesd_process_and_kill_the_rest(uuid, false);
    if (gpid > 0) {
        write_pid_file_for_uuid(uuid, gpid);
        DEBUG_LOG("Grandchild PID ", gpid, " written to pidfile");
    } else {
        DEBUG_LOG("Warning: unable to determine grandchild PID for ", uuid);
    }

    // Wait up to ~5s for PID file to appear
    DEBUG_LOG("[beesstart] before wait_for_pid_file... ", bk_util::current_timestamp());
    wait_for_pid_process_to_start(get_pid_path(uuid), 10, 300000);
    DEBUG_LOG("[beesstart] after wait_for_pid_file... ", bk_util::current_timestamp());

    // Check status via beesstatus (will internally fallback to grab_one_beesd_process_and_kill_the_rest if needed)
    std::string status = beesstatus(uuid);

    if (status != "running" && status != "running (with logging)") {
        DEBUG_LOG("Beesd failed to start for UUID: ", uuid);
        return false;
    } else {
        DEBUG_LOG("Beesd successfully running for ", uuid);
    }

    // Create "Started with x.xx GiB free" file ONLY at success
    bk_mgmt::create_started_with_n_gb_file(uuid);

    return true;
}

// Stop beesd daemon using UUID
bool
bk_mgmt::beesstop(const std::string& uuid)
{
    // Check for root privileges
    if (!bk_util::is_root()) {
        std::cerr << "Error: beesstop requires root privileges." << std::endl;
        return false;
    }

    // If already stopped or never configured, consider it success
    std::string status = beesstatus(uuid);
    if (status == "stopped" || status == "unconfigured") {
        DEBUG_LOG("beesstop: UUID ", uuid, " is already stopped or unconfigured");
        return true;
    }

    std::string pidfile = get_pid_path(uuid);
    bool stopped = false;

    // First method: PID file approach
    if (check_if_pidfile_process_is_running(pidfile)) {
        kill_pidfile_process(pidfile);
        if (wait_for_pid_process_to_stop(pidfile)) {
            stopped = true;
        } else {
            kill_pidfile_process(pidfile);
            stopped = wait_for_pid_process_to_stop(pidfile);
            if (!stopped) {
                std::cerr << "Beesd process for UUID " << uuid << " failed to stop." << std::endl;
            }
        }
    }

    // Fallback to search and kill by manual PID find
    // if PID file method failed
    if (!stopped) {
        if (!bk_util::is_uuid(uuid)) {
            std::cerr << "Invalid UUID: " << uuid << std::endl;
            return false;
        }
        pid_t pid = grab_one_beesd_process_and_kill_the_rest(uuid, true);
        if (pid > 0 && kill(pid, 0) == 0) { // double-check still running
            write_pid_file_for_uuid(uuid, pid);
            if (check_if_pidfile_process_is_running(pidfile)) {
                kill_pidfile_process(pidfile);
                stopped = wait_for_pid_process_to_stop(pidfile);
            }
        } else {
            DEBUG_LOG("No beesd process found for UUID ", uuid);
        }
    }

    // Clean up PID file
    clean_pid_file_for_uuid (uuid);

    // Clean logs
    clear_log_file_for_uuid (uuid);

    return stopped;
}

// Restart beesd daemon
bool
bk_mgmt::beesrestart(const std::string& uuid)
{
    if (beesstatus(uuid) == "running") {
        // Only stop if it's running
        if (!beesstop(uuid)) {
            DEBUG_LOG("Failed to stop beesd for UUID ", uuid);
            return false;
        }
        sleep(1); // Wait a bit before starting
    }

    // Start the daemon (whether it was stopped or just stopped now)
    return beesstart(uuid);
}


// Check daemon status using UUID
std::string
bk_mgmt::beesstatus(const std::string& uuid)
{
    std::string pidfile = get_pid_path(uuid);

    // 1. Check logger processes first
    // No-op: logging removed temporarily

    // 2. Check PID file
    if (check_if_pidfile_process_is_running(pidfile) && verify_beesd_process(pidfile)) {
        return std::string("running") + (bk_util::file_exists(get_log_path(uuid)) ? " (with logging)" : "");
    } else {
        clean_pid_file_for_uuid (uuid);
    }

    // 3. Fallback to system process check
    pid_t worker_pid = grab_one_beesd_process_and_kill_the_rest(uuid, true);
    if (worker_pid > 0 && verify_beesd_process(worker_pid)) {
        // Update PID file
        std::ofstream out(pidfile);
        if (out) {
            out << worker_pid;
            DEBUG_LOG("PID ", worker_pid, " written to ", pidfile);
        }
        return "running";
    }

    // 4. Check if configuration file exists (after running checks)
    if (bk_mgmt::btrfstat(uuid) == "") {
        return "unconfigured";
    }

    // 5. If not running and config exists
    return "stopped";
}

void
bk_mgmt::beeslog (const std::string& uuid)
{
    // No-op: logging temporarily removed
}


// Clean up PID file
// Clean up PID file
void
bk_mgmt::beescleanlogfiles(const std::string& uuid)
{
    clear_log_file_for_uuid(uuid);

    // Only clean PID file if beesd is not running
    std::string status = beesstatus(uuid);
    if (status == "stopped" || status == "unconfigured") {
        clean_pid_file_for_uuid (uuid);
    } else {
        DEBUG_LOG("beesd is running, not cleaning PID file for UUID ", uuid);
    }
}