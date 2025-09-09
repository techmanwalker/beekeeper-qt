#include "../../include/beekeeper/beesdmgmt.hpp"
#include "../../include/beekeeper/btrfsetup.hpp"
#include "../../include/beekeeper/internalaliases.hpp" // required for bk_mgmt and bk_util
#include "../../include/beekeeper/debug.hpp"
#include "../../include/beekeeper/util.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

// -- LOGGING --

// Global map to track logger PIDs
static std::map<std::string, pid_t> logger_pids;
static std::mutex logger_mutex;

// Helper: Get PID file path for UUID
static std::string
get_pid_path (const std::string& uuid)
{
    return "/var/run/beesd-" + uuid + ".pid";
}

// Helper: Get log directory path
static std::string
get_log_dir ()
{
    return "/var/log/beesd/";
}

// Helper: Get log file path
static std::string
get_log_path (const std::string& uuid)
{
    return "/var/log/beesd/" + uuid + ".log";
}

// Helper: Create log directory if needed
static void
ensure_log_dir ()
{
    std::string log_dir = get_log_dir();
    if (!bk_util::file_exists(log_dir)) {
        fs::create_directories(log_dir);
        fs::permissions(log_dir, fs::perms::owner_all);
    }
}

// Helper: Start logger process
static pid_t
start_logger (const std::string& uuid, const std::string& log_path)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Logger process
        close(pipefd[1]);  // Close write end
        
        // Open log file (create new/truncate existing)
        int log_fd = open(log_path.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);
        if (log_fd < 0) {
            exit(EXIT_FAILURE);
        }
        
        char buffer[4096];
        ssize_t count;
        
        // Perform the read operation
        while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            // Write the data we just read
            write(log_fd, buffer, count);
        }
        
        close(pipefd[0]);
        close(log_fd);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) {
        // Parent process - start beesd
        close(pipefd[0]);  // Close read end
        
        pid_t beesd_pid = fork();
        if (beesd_pid == 0) {
            // Beesd process
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
            
            execlp("beesd", "beesd", uuid.c_str(), (char*)NULL);
            exit(EXIT_FAILURE);
        } else {
            close(pipefd[1]);
            return pid;  // Return logger PID
        }
    }
    return -1;
}

// Stop logger process
static void
stop_logger(const std::string& uuid)
{
    std::lock_guard<std::mutex> lock(logger_mutex);
    auto it = logger_pids.find(uuid);
    if (it != logger_pids.end()) {
        kill(-it->second, SIGTERM);  // Kill entire process group
        waitpid(it->second, nullptr, 0);
        logger_pids.erase(it);
    }
}

// -- END LOGGING --

// Helper: Clean up PID file for UUID
static void
clean_pid_file (const std::string& uuid)
{
    std::string pidfile = get_pid_path(uuid);
    if (bk_util::file_exists(pidfile)) {
        fs::remove(pidfile);
    }
}

// Helper: Find the PID of the bees worker process for a given UUID
static pid_t
find_bees_worker_pid (const std::string& uuid)
{
    std::string cmd = "ps aux | grep -e 'bees' | grep '" + uuid + "' | grep -v beekeeper | grep -v beesstatus | grep -v grep";
    std::string output = bk_util::exec_command(cmd.c_str()).stdout_str;
    
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
        // String copy to cut it freely
        std::string pid_str = line;

        // Shove off the first token
        size_t start = line.find_first_of(" \t");
        if (start == std::string::npos) {
            return 0;  // Empty line
        }
        pid_str = pid_str.substr(start);

        // Trim whitespace
        pid_str = bk_util::trim_string(pid_str);
        
        // Find first whitespace after PID to shove off everything else
        size_t end = pid_str.find_first_of(" \t", start);
        if (end == std::string::npos) {
            end = line.length();
        }
        pid_str = pid_str.substr(0, end);
        
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
static bool
verify_beesd_process(pid_t pid) {
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
        DEBUG_LOG("Command line for PID ", pid, ": ", cmdline);
        return cmdline.find("bees") != std::string::npos;
    }
    return false;
}

// Start beesd daemon using UUID
bool
bk_mgmt::beesstart (const std::string& uuid, bool enable_logging)
{
    DEBUG_LOG("==== Starting beesd for UUID: ", uuid, " ====");
    DEBUG_LOG("Logging enabled: ", (enable_logging ? "yes" : "no"));
    
    // Check for root privileges
    if (!bk_util::is_root()) {
        std::cerr << "Error: beesstsart requires root privileges. Please run with sudo." << std::endl;
        return false;
    }

    // Check if already running
    std::string status = bk_mgmt::beesstatus(uuid);
    DEBUG_LOG("Current status: ", status);
    if (status == "running") {
        std::cerr << "Warning: There is already a running instance for " 
                  << uuid << ". Ignoring start request." << std::endl;
        return true;
    }
    
    // Clean up any old PID file
    clean_pid_file(uuid);
    ensure_log_dir();

    // Always remove existing log file before starting
    std::string log_path = get_log_path(uuid);
    if (bk_util::file_exists(log_path)) {
        DEBUG_LOG("Found existing log file - removing");
        try {
            if (fs::remove(log_path)) {
                DEBUG_LOG("Successfully removed old log file");
            } else {
                DEBUG_LOG("Failed to remove log file");
                std::cerr << "Warning: Failed to remove existing log file: " 
                          << log_path << std::endl;
            }
        } catch (const fs::filesystem_error& e) {
            DEBUG_LOG("Exception removing log: ", e.what());
            std::cerr << "Warning: Failed to remove log file: " 
                      << e.what() << std::endl;
        }
    } else {
        DEBUG_LOG("No existing log file found");
    }
    
    // Only create logs when explicitly enabled due to their huge size
    // (300 MB+ for short runs)
    if (enable_logging) {
        // Start logger process
        std::lock_guard<std::mutex> lock(logger_mutex);
        pid_t logger_pid = start_logger(uuid, log_path);
        if (logger_pid <= 0) {
            DEBUG_LOG("Failed to start logger");
            return false;
        }
        
        logger_pids[uuid] = logger_pid;
        DEBUG_LOG("Logger started with PID: ", logger_pid);
        
        // Write PID to file
        std::string pidfile = get_pid_path(uuid);
        std::ofstream out(pidfile);
        if (out) {
            out << logger_pid;
            DEBUG_LOG("PID written to file: ", pidfile);
        } else {
            DEBUG_LOG("Failed to write PID file: ", pidfile);
            std::cerr << "Warning: Failed to create PID file: " << pidfile << std::endl;
        }
        
        return true;
    } else {
        // Start without logging
        pid_t pid = fork();
        if (pid < 0) {
            DEBUG_LOG("fork() failed: ", strerror(errno));
            return false;
        } else if (pid == 0) {
            // Child process
            // Create a temporary log file for startup errors
            std::string error_log = "/tmp/beesd-start-" + uuid + ".log";
            int log_fd = open(error_log.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);
            if (log_fd >= 0) {
                dup2(log_fd, STDOUT_FILENO);
                dup2(log_fd, STDERR_FILENO);
                close(log_fd);
            }
            
            execlp("beesd", "beesd", uuid.c_str(), (char*)NULL);
            
            // If we get here, exec failed
            DEBUG_LOG("execlp() failed: ", strerror(errno));
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            std::string pidfile = get_pid_path(uuid);
            std::ofstream out(pidfile);
            if (out) {
                out << pid;
                DEBUG_LOG("PID ", pid, " written to ", pidfile);
            } else {
                DEBUG_LOG("Failed to write PID file: ", pidfile);
                std::cerr << "Warning: Failed to create PID file: " << pidfile << std::endl;
            }
            
            // Verify process is running after 300ms for 5 times
            bool is_running = false;
            pid_t worker_pid = 0;

            for (int i = 0; i < 5; i++) {
                usleep(300000);
                
                worker_pid = find_bees_worker_pid(uuid);
                if (worker_pid > 0) {
                    DEBUG_LOG("Found worker PID: ", worker_pid);
                    is_running = true;
                    break;
                }
            }

            if (is_running) {
                // Update PID file with actual worker PID
                std::ofstream out(pidfile);
                if (out) {
                    out << worker_pid;
                    DEBUG_LOG("Worker PID ", worker_pid, " written to ", pidfile);
                } else {
                    DEBUG_LOG("Failed to update PID file with worker PID: ", pidfile);
                }
                return true;
            } else {
                DEBUG_LOG("Process verification failed");
                clean_pid_file(uuid);
                return false;
            }
            
            return true;
        }
        
    
    }
}

// Stop beesd daemon using UUID
bool
bk_mgmt::beesstop (const std::string& uuid)
{
    // Check for root privileges
    if (!bk_util::is_root()) {
        std::cerr << "Error: beesstop requires root privileges. Please run with sudo." << std::endl;
        return false;
    }

    std::string pidfile = get_pid_path(uuid);
    bool stopped = false;

    std::lock_guard<std::mutex> lock(logger_mutex);
    if (logger_pids.find(uuid) != logger_pids.end()) {
        pid_t pid = logger_pids[uuid];
        kill(pid, SIGTERM);

        // Non-blocking waitpid loop
        int status;
        pid_t result;
        for (int i = 0; i < 5; i++) { // 5 seconds max wait
            result = waitpid(pid, &status, WNOHANG);
            if (result > 0) {
                stopped = true;
                break;
            }
            usleep(200000); // check every 0.2 seconds
        }

        if (!stopped) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0); // ensure termination
            stopped = true;
        }

        logger_pids.erase(uuid);
        return true;
    }

    // First method: PID file approach
    if (bk_util::file_exists(pidfile)) {
        std::ifstream in(pidfile);
        pid_t pid;
        if (in >> pid) {
            if (kill(pid, SIGTERM) == 0) {
                int status;
                pid_t result;
                stopped = false;
                for (int i = 0; i < 5; i++) { // 5 seconds max
                    result = waitpid(pid, &status, WNOHANG);
                    if (result > 0) {
                        stopped = true;
                        break;
                    }
                    usleep(200000); // 0.2s
                }
            }

            if (!stopped) {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, 0);
                stopped = true;
            }
        }
    }

    // Clean up PID file
    clean_pid_file(uuid);

    // Fallback to pkill if PID file method failed
    if (!stopped) {
        std::string cmd = "pkill -f 'beesd .* " + uuid + "'";
        int status = std::system(cmd.c_str());
        stopped = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    return stopped;
}

// Restart beesd daemon
bool
bk_mgmt::beesrestart (const std::string& uuid)
{
    return bk_mgmt::beesstop(uuid) && sleep(1) == 0 && beesstart(uuid);
}

// Check daemon status using UUID
std::string
bk_mgmt::beesstatus(const std::string& uuid)
{
    std::string pidfile = get_pid_path(uuid);
    std::lock_guard<std::mutex> lock(logger_mutex);

    // 1. Check logger processes first
    auto logger_it = logger_pids.find(uuid);
    if (logger_it != logger_pids.end()) {
        pid_t logger_pid = logger_it->second;
        if (kill(logger_pid, 0) == 0) {
            return "running (with logging)";
        } else {
            // Clean up stale logger entry
            logger_pids.erase(logger_it);
        }
    }

    // 2. Check PID file
    if (bk_util::file_exists(pidfile)) {
        std::ifstream in(pidfile);
        pid_t pid;
        if (in >> pid) {
            if (kill(pid, 0) == 0 && verify_beesd_process(pid)) {
                return "running";
            } else {
                clean_pid_file(uuid);
            }
        }
    }

    // 3. Fallback to system process check
    pid_t worker_pid = find_bees_worker_pid(uuid);
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
    std::string log_path = get_log_path(uuid);
    std::string status = bk_mgmt::beesstatus(uuid);
    
    if (status.find("running") != std::string::npos) {
        // Daemon is running
        if (bk_util::file_exists(log_path)) {
            std::system(("tail -f \"" + log_path + "\"").c_str());
        } else {
            std::cerr << "Warning: Logging is disabled. No live logs available.\n";
            std::cerr << "Beesd is running and deduplicating your files.\n";
            std::cerr << "Start with --enable-logging to see live logs.\n";
        }
    } else {
        // Daemon is stopped
        if (bk_util::file_exists(log_path)) {
            std::cerr << "Warning: Beesd is not running. Printing last session log...\n";
            sleep(3);
            std::system(("cat \"" + log_path + "\"").c_str());
        } else {
            std::cerr << "Beesd is not running and there is no log information available.\n";
            std::cerr << "Start with --enable-logging to enable logging for future sessions.\n";
        }
    }
}


// Clean up PID file
void
bk_mgmt::beescleanlogfiles (const std::string& uuid)
{
    clean_pid_file(uuid);
}