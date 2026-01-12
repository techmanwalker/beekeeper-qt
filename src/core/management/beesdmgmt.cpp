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
#include <thread>
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


// Static functions (just because semantics)

// Returns all running bees worker PIDs for this uuid
static std::vector<pid_t>
find_bees_workers_for_uuid(const std::string &uuid)
{
    return bk_mgmt::find_beesd_processes(uuid, true); // this already searches for: "bees <mountpoint>"
}


// Returns true if at least one bees worker exists
static bool
is_bees_worker_running(const std::string &uuid)
{
    auto workers = find_bees_workers_for_uuid(uuid);
    return !workers.empty();
}


// Wait up to timeout_ms for a bees worker to appear
static bool
wait_for_bees_worker(const std::string &uuid, int timeout_ms = 3000)
{
    const int step_ms = 100;
    int waited = 0;

    while (waited < timeout_ms) {
        if (is_bees_worker_running(uuid))
            return true;

        usleep(step_ms * 1000);
        waited += step_ms;
    }

    return false;
}




// --------------- THE TOP GLOBALS (literally) ---------------





// Start beesd daemon using UUID
bool
bk_mgmt::beesstart(const std::string &uuid)
{
    DEBUG_LOG("beesstart(", uuid, ")");

    // ------------------------------------------------------------
    // 1) Must have a valid config file
    // ------------------------------------------------------------
    const std::string config_path = bk_mgmt::btrfstat(uuid);
    if (config_path.empty() || !std::filesystem::exists(config_path)) {
        DEBUG_LOG("No config file for uuid ", uuid, ", refusing to start bees.");
        return false;
    }

    // ------------------------------------------------------------
    // 2) If already running according to our own status API → done
    // ------------------------------------------------------------
    if (bk_mgmt::beesstatus(uuid) == "running") {
        DEBUG_LOG("bees already running according to beesstatus()");
        bk_mgmt::create_started_with_n_gb_file(uuid);
        return true;
    }

    // ------------------------------------------------------------
    // 3) Ensure only one bees worker exists; if any, return true
    // ------------------------------------------------------------
    pid_t worker_pid = bk_mgmt::grab_one_beesd_process_and_kill_the_rest(uuid);
    if (worker_pid > 0) {
        DEBUG_LOG("bees worker already running for uuid ", uuid, ", PID ", worker_pid);

        // Write PID file
        write_pid_file_for_uuid(uuid, worker_pid);

        bk_mgmt::create_started_with_n_gb_file(uuid);
        return true;
    }


    // ------------------------------------------------------------
    // 4) Launch via beesd (double-fork trampoline)
    //     beesd may die; bees must survive
    // ------------------------------------------------------------
    DEBUG_LOG("Launching beesd for uuid ", uuid);

    pid_t pid = fork();
    if (pid < 0) {
        DEBUG_LOG("First fork failed");
        return false;
    }

    if (pid == 0) {
        // child #1
        pid_t pid2 = fork();
        if (pid2 < 0) {
            _exit(1);
        }

        if (pid2 == 0) {
            // child #2 → this becomes beesd
            execl(
                "/usr/lib/bees/beesd",
                "beesd",
                uuid.c_str(),
                nullptr
            );

            // If exec failed
            _exit(1);
        }

        // child #1 exits immediately
        _exit(0);
    }

    // parent continues

    // ------------------------------------------------------------
    // 5) Wait for bees worker to appear
    // ------------------------------------------------------------
    DEBUG_LOG("Waiting for bees worker to appear for uuid ", uuid);

    if (!wait_for_bees_worker(uuid, 5000)) {
        DEBUG_LOG("Timed out waiting for bees worker for uuid ", uuid);
        return false;
    }

    // ------------------------------------------------------------
    // 6) Success: worker exists
    // ------------------------------------------------------------
    DEBUG_LOG("bees worker started successfully for uuid ", uuid);

    // Update pidfile with the real worker
    auto workers = find_bees_workers_for_uuid(uuid);
    if (!workers.empty()) {
        write_pid_file_for_uuid(uuid, workers[0]);
    }

    // Ensure GUI has a baseline for saved bytes
    bk_mgmt::create_started_with_n_gb_file(uuid);

    return true;
}

// Stop beesd/bees processes using UUID
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

    // ------------------------------------------------------------
    // 1) Find all bees/beesd processes containing this UUID
    // ------------------------------------------------------------
    std::vector<std::string> ps_lines = bk_util::get_process_lines("bees", uuid);
    if (ps_lines.empty()) {
        DEBUG_LOG("No bees processes found for UUID ", uuid);
        clean_pid_file_for_uuid(uuid);
        clear_log_file_for_uuid(uuid);
        return true;
    }

    // ------------------------------------------------------------
    // 2) Terminate each process aggressively
    // ------------------------------------------------------------
    constexpr auto wait_time = std::chrono::seconds(15);
    constexpr auto poll_interval = std::chrono::milliseconds(200);

    for (const auto &line : ps_lines) {
        pid_t pid = std::stoll(bk_util::get_second_token(line));
        if (pid <= 0) {
            DEBUG_LOG("Failed to parse PID from line: ", line);
            continue;
        }

        // Send SIGTERM first
        kill(pid, SIGTERM);

        // Wait up to 15 seconds for graceful exit
        auto waited = std::chrono::milliseconds(0);
        while (kill(pid, 0) == 0 && waited < wait_time) {
            std::this_thread::sleep_for(poll_interval);
            waited += poll_interval;
        }

        // If still alive, force SIGKILL
        if (kill(pid, 0) == 0) {
            std::cerr << "Process PID " << pid << " for UUID " << uuid
                      << " did not exit after SIGTERM, sending SIGKILL" << std::endl;
            kill(pid, SIGKILL);
        } else {
            DEBUG_LOG("Process PID ", pid, " terminated gracefully for UUID ", uuid);
        }
    }

    // ------------------------------------------------------------
    // 3) Cleanup PID file and logs
    // ------------------------------------------------------------
    clean_pid_file_for_uuid(uuid);
    clear_log_file_for_uuid(uuid);

    return true;
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

    // 1. Check PID file
    if (check_if_pidfile_process_is_running(pidfile)) {
        return "running";
    }

    // 2. Find WORKER bees by mountpoint
    auto worker_pids = find_beesd_processes(uuid, true);   // this finds "bees <mount>"
    if (!worker_pids.empty()) {
        pid_t pid = worker_pids[0];
        write_pid_file_for_uuid(uuid, pid);
        return "running";
    }

    // 3. Config exists?
    if (bk_mgmt::btrfstat(uuid).empty()) {
        return "unconfigured";
    }

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