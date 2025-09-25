#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/debug.hpp"

#include <filesystem>
#include <iostream>
#include <sys/wait.h>

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
        fs::remove(pidfile);
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
 * @brief Keep only one beesd process for a given UUID, killing the rest.
 *
 * This function ensures that for a given `uuid` there is at most one
 * active beesd process running (manager or worker). Running multiple
 * instances on the same UUID doesn't make sense because they operate
 * over the same underlying data and can waste CPU and memory resources.
 *
 * @param uuid The unique identifier for the beesd instance.
 * @param act_against_the_worker_pids If false, act on the manager process;
 *        if true, act on worker processes instead.
 * @return The PID of the first surviving process, or 0 if none exist.
 *
 * @note Current implementation simply kills all extra processes and keeps
 *       the first one. Future improvements could introduce threading and
 *       worker control as a "deduplication performance profile" preset.
 */
pid_t
bk_mgmt::grab_one_beesd_process_and_kill_the_rest(
    const std::string &uuid,
    bool act_against_the_worker_pids
)
{
    auto pids = act_against_the_worker_pids
                    ? find_beesd_processes(uuid, true)
                    : find_beesd_processes(uuid);

    if (pids.empty())
        return 0;

    // Keep the first PID, kill the rest
    for (size_t i = 1; i < pids.size(); ++i)
    {
        pid_t extra_pid = pids[i];
        kill_process(extra_pid); // uses default signal/wait parameters
    }

    return pids[0]; // return the surviving process PID
}