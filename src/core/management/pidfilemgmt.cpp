#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <sys/wait.h>
#include <thread>

namespace fs = std::filesystem;

/**
 * @brief Reads a PID from a pidfile.
 *
 * If the pidfile is empty or contains 0, it will be deleted.
 *
 * @param path Filesystem path to the pidfile
 * @return PID from the file, or 0 if invalid or file is empty
 */
pid_t
bk_mgmt::read_pidfile(const std::string &path)
{
    std::ifstream pidfile(path);
    if (!pidfile.is_open()) {
        return 0;
    }

    std::string line;
    std::getline(pidfile, line);
    pidfile.close();

    if (line.empty()) {
        ::unlink(path.c_str());
        return 0;
    }

    std::istringstream iss(line);
    pid_t pid = 0;
    iss >> pid;

    if (pid == 0) {
        ::unlink(path.c_str());
    }

    return pid;
}

/**
 * @brief Reads the PIDfile for a given UUID.
 *
 * @param uuid UUID string to locate the pidfile
 * @return PID from the file, or 0 if invalid or file is empty
 */
pid_t
bk_mgmt::read_pidfile_for_uuid(const std::string &uuid)
{
    std::string path = bk_mgmt::get_pid_path(uuid);
    return read_pidfile(path);
}



void
bk_mgmt::remove_pidfile_path(const std::string &pidfile_path)
{
    if (pidfile_path.empty()) return;
    if (bk_util::file_exists(pidfile_path)) {
        try {
            fs::remove(pidfile_path);
            DEBUG_LOG("[pidfile] removed pidfile path: ", pidfile_path);
        } catch (const std::exception &e) {
            DEBUG_LOG("[pidfile] failed removing pidfile path: ", pidfile_path, " err: ", e.what());
        }
    }
}

// Helper: Get PID file path for UUID
std::string
bk_mgmt::get_pid_path (const std::string& uuid)
{
    return "/var/run/beesd-" + uuid + ".pid";
}

// Helper: Clean up PID file for UUID
void
bk_mgmt::clean_pid_file_for_uuid (const std::string& uuid)
{
    std::string pidfile = bk_mgmt::get_pid_path(uuid);
    if (bk_util::file_exists(pidfile)) {
        remove_pidfile_path(pidfile);
    }
}

// --- check a PID directly ---
bool
bk_mgmt::check_if_pid_process_is_running(pid_t pid)
{
    if (pid <= 0)
        return false;

    // kill with signal 0 returns 0 if process exists and permission is OK
    return kill(pid, 0) == 0;
}

// --- check a pidfile ---
bool
bk_mgmt::check_if_pidfile_process_is_running(const std::string &pidfile)
{
    if (!bk_util::file_exists(pidfile))
        return false;

    std::ifstream in(pidfile);
    pid_t pid = 0;
    if (!(in >> pid) || pid <= 0) {
        // Empty or invalid pidfile → remove stale file
        remove_pidfile_path(pidfile);
        return false;
    }

    bool running = check_if_pid_process_is_running(pid);

    if (!running) {
        remove_pidfile_path(pidfile);
    }

    return running;
}

// Wait for pid file process to start or stop

bool
bk_mgmt::wait_for_pid_process_to_start(pid_t proc,
                                       int retries,
                                       int usleep_microseconds)
{
    for (int attempt = 0; attempt < retries; ++attempt) {
        if (check_if_pid_process_is_running(proc)) {
            return true;
        }
        usleep(usleep_microseconds);
    }
    return false;
}

bool
bk_mgmt::wait_for_pid_process_to_start(const std::string &pidfile,
                                       int retries,
                                       int usleep_microseconds)
{
    pid_t pid = read_pidfile(pidfile);
    if (pid == 0) {
        return false;
    }

    return wait_for_pid_process_to_start(pid, retries, usleep_microseconds);
}

/**
 * @brief Waits for a PID to stop running.
 *
 * Checks repeatedly if the process is still alive.
 *
 * @param proc PID to monitor
 * @param retries Number of attempts
 * @param usleep_microseconds Sleep duration between attempts
 * @return true if the process stopped within retries, false otherwise
 */
bool
bk_mgmt::wait_for_pid_process_to_stop(pid_t proc,
                                      int retries,
                                      int usleep_microseconds)
{
    for (int i = 0; i < retries; ++i) {
        if (!check_if_pid_process_is_running(proc)) {
            return true;
        }
        usleep(usleep_microseconds);
    }
    return false;
}

/**
 * @brief Waits for the process contained in a pidfile to stop running.
 *
 * If the pidfile contains 0, returns true immediately since
 * there's no valid process to wait for.
 *
 * @param pidfile Path to the pidfile
 * @param retries Number of attempts
 * @param usleep_microseconds Sleep duration between attempts
 * @return true if the process stopped, false otherwise
 */
bool
bk_mgmt::wait_for_pid_process_to_stop(const std::string &pidfile,
                                      int retries,
                                      int usleep_microseconds)
{
    pid_t pid = read_pidfile(pidfile);
    if (pid == 0) {
        return true; // Nothing to wait for
    }

    return wait_for_pid_process_to_stop(pid, retries, usleep_microseconds);
}

void
bk_mgmt::write_pid_file_for_uuid(const std::string &uuid, pid_t pid)
{
    std::string pidfile = get_pid_path(uuid);
    std::ofstream out(pidfile);
    if (out) {
        out << pid;
        DEBUG_LOG("PID ", pid, " written to ", pidfile);
    } else {
        DEBUG_LOG("Failed to write PID file: ", pidfile);
        std::cerr << "Warning: Failed to create PID file: " << pidfile << std::endl;
    }
}

// --- core function: kills a pid with waiting and optional SIGKILL ---
bool
bk_mgmt::kill_process(pid_t pid, int sig, int wait_retries, int wait_usleep)
{
    bool killed = false;

    // First attempt: send the requested signal
    if (kill(pid, sig) == 0) {
        for (int i = 0; i < wait_retries; ++i) {
            if (!beekeeper::management::check_if_pid_process_is_running(pid)) { // assumes helper that checks PID directly
                killed = true;
                break;
            }
            usleep(wait_usleep);
        }
    }

    // Force kill if still running
    if (!killed) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        killed = true;
    }

    return killed;
}

// --- high-level function: read pidfile, kill pid, cleanup ---
bool
bk_mgmt::kill_pidfile_process(const std::string &pidfile, int sig, int wait_retries, int wait_usleep)
{
    if (!bk_util::file_exists(pidfile))
        return false;

    std::ifstream in(pidfile);
    pid_t pid;
    if (!(in >> pid))
        return false;

    bool killed = kill_process(pid, sig, wait_retries, wait_usleep);

    // Cleanup pidfile
    remove_pidfile_path(pidfile);

    return killed;
}

/**
 * @brief Get all lines of 'ps aux' output that contain both the process name and UUID
 *
 * This imitates `ps aux | grep <process_name> | grep <uuid> | grep -v grep`.
 *
 * @param process_name The name of the process to search for (e.g., "bees")
 * @param uuid The UUID string to match in the command line
 * @return std::vector<std::string> Lines of 'ps aux' output matching both criteria
 */
std::vector<std::string>
bk_util::get_process_lines(const std::string &process_name, const std::string &uuid)
{
    std::vector<std::string> result;

    // Execute 'ps aux'
    command_streams streams = exec_command("ps", "aux");
    std::string &output = streams.stdout_str;

    if (output.empty())
        return result;

    size_t start = 0;
    while (start < output.size())
    {
        // Find next newline
        size_t end = output.find('\n', start);
        if (end == std::string::npos)
            end = output.size();

        std::string line = output.substr(start, end - start);

        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Check if line contains both process_name and uuid
        if (line.find(process_name) != std::string::npos &&
            line.find(uuid) != std::string::npos)
        {
            result.push_back(line);
        }

        start = end + 1; // move past newline
    }

    return result;
}


/**
 * @brief Ensure only one bees process exists for a given UUID
 *
 * This function enumerates all running bees processes whose command line
 * contains the specified UUID. It keeps the first process alive and kills
 * all the other duplicates. This guarantees that at most one bees worker
 * exists per UUID, imitating `ps aux | grep bees | grep -v grep | grep <uuid>`.
 *
 * @param uuid The UUID identifying the bees mount/process
 * @return pid_t The PID of the process kept alive, or -1 if none found
 */
pid_t
bk_mgmt::grab_one_beesd_process_and_kill_the_rest(const std::string &uuid)
{
    // Step 1: Get all lines of 'ps aux' output containing 'bees' but not 'grep'
    std::vector<std::string> ps_lines = bk_util::get_process_lines("bees", uuid);

    if (ps_lines.empty())
    {
        // No bees processes running for this UUID
        DEBUG_LOG("No bees processes found for UUID ", uuid);
        return -1; // indicate no process found
    }

    // Step 2: Iterate through each line, extract PID using bk_util helper
    bool first = true;
    pid_t kept_pid = -1;

    for (const auto &line : ps_lines)
    {
        pid_t pid = std::stoll(bk_util::get_second_token(line)); // second token in ps aux is PID
        if (pid <= 0)
        {
            DEBUG_LOG("Failed to parse PID from line: ", line);
            continue;
        }

        if (first)
        {
            // Keep the first process alive
            DEBUG_LOG("Keeping bees process alive for UUID ", uuid, ": PID ", pid);
            kept_pid = pid;
            first = false;
            continue;
        }

        // Kill the rest
        DEBUG_LOG("Killing duplicate bees process for UUID ", uuid, ": PID ", pid);
        if (kill(pid, SIGTERM) != 0)
        {
            DEBUG_LOG("Failed to terminate PID ", pid, ": ", strerror(errno));
        }
        else
        {
            // Optional: wait a small moment and SIGKILL if still alive
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (kill(pid, 0) == 0) // process still exists
            {
                DEBUG_LOG("Process still alive, sending SIGKILL to PID ", pid);
                kill(pid, SIGKILL);
            }
        }
    }

    // Step 3: Update GUI with started file info
    if (kept_pid > 0)
        bk_mgmt::create_started_with_n_gb_file(uuid);

    return kept_pid;
}