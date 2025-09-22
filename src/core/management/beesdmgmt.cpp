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
 * @brief Find the PID of the bees daemon or its worker process for a given UUID.
 *
 * This function searches for the process corresponding to the given UUID.
 * Depending on the `find_worker_pid` flag, it can either return:
 *   - the main beesd daemon PID (`find_worker_pid == false`)
 *   - the child worker bees process PID (`find_worker_pid == true`)
 *
 * @param uuid The UUID associated with the beesd process.
 * @param find_worker_pid If true, search for the child worker bees process; otherwise, search for the main beesd daemon.
 * @return The PID of the matching process, or 0 if not found.
 */
pid_t
bk_mgmt::find_beesd_process (const std::string& uuid, bool find_worker_pid)
{
    std::string cmd = std::string("ps aux | grep -e ") + (find_worker_pid ? "bees" : "beesd") +
                    " | grep '" + uuid + "' | grep -v beekeeper | grep -v beesstatus | grep -v defunct | grep -v grep";

    std::string output = bk_util::exec_command_shell(cmd.c_str()).stdout_str;
    
    if (output.empty()) {
        DEBUG_LOG("No processes found for UUID: ", uuid);
        return 0;
    }

    DEBUG_LOG("Process search output: ", output);
    
    // Process the output to find the first PID
    std::istringstream iss(output);
    std::string line;
    
    // Only process the first line of output
    if (std::getline(iss, line)) {
        std::string pid_str = bk_util::get_second_token(line);
        
        // Extract PID substring
        DEBUG_LOG("Found candidate PID: ", pid_str);
        
        try {
            pid_t pid = std::stoi(pid_str);
            DEBUG_LOG("Parsed PID: ", pid);
            return pid;
        } catch (...) {
            DEBUG_LOG("Failed to parse PID from: '", pid_str, "'");
            return 0;
        }
    }
    
    return 0;
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
    pid_t gpid = bk_mgmt::find_beesd_process(uuid, false);
    if (gpid > 0) {
        write_pid_file_for_uuid(uuid, gpid);
        DEBUG_LOG("Grandchild PID ", gpid, " written to pidfile");
    } else {
        DEBUG_LOG("Warning: unable to determine grandchild PID for ", uuid);
    }

    // Wait up to ~5s for PID file to appear
    DEBUG_LOG("[beesstart] before wait_for_pid_file... ", bk_util::current_timestamp());
    wait_for_pid_file_process_to_start(get_pid_path(uuid), 10, 300000);
    DEBUG_LOG("[beesstart] after wait_for_pid_file... ", bk_util::current_timestamp());

    // Check status via beesstatus (will internally fallback to find_beesd_process if needed)
    std::string status = beesstatus(uuid);

    if (status != "running" && status != "running (with logging)") {
        DEBUG_LOG("Beesd failed to start for UUID: ", uuid);
        return false;
    }

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
        if (wait_for_pid_file_process_to_stop(pidfile)) {
            stopped = true;
        } else {
            kill_pidfile_process(pidfile);
            stopped = wait_for_pid_file_process_to_stop(pidfile);
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
        pid_t pid = find_beesd_process(uuid, true);
        if (pid > 0 && kill(pid, 0) == 0) { // double-check still running
            write_pid_file_for_uuid(uuid, pid);
            if (check_if_pidfile_process_is_running(pidfile)) {
                kill_pidfile_process(pidfile);
                stopped = wait_for_pid_file_process_to_stop(pidfile);
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
    pid_t worker_pid = find_beesd_process(uuid, true);
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