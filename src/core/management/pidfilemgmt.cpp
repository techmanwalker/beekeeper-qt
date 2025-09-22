#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/debug.hpp"

#include <filesystem>
#include <iostream>
#include <sys/wait.h>

namespace fs = std::filesystem;

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

bool
bk_mgmt::check_if_pidfile_process_is_running(const std::string &pidfile)
{
    // Check if the pidfile exists
    if (!bk_util::file_exists(pidfile)) return false;

    std::ifstream in(pidfile);
    pid_t pid = 0;
    if (!(in >> pid) || pid <= 0) {
        // Empty or invalid pidfile → immediately false
        remove_pidfile_path (pidfile);
        return false;
    }

    // kill with signal 0 checks if the process exists
    if (kill(pid, 0) == 0 && verify_beesd_process(pid)) {
        return true;
    }

    // Cleanup stale pidfile
    remove_pidfile_path (pidfile);
    return false;
}

// Wait for pid file process to start or stop

bool
bk_mgmt::wait_for_pid_file_process_to_start(const std::string &pidfile,
                                            int retries,
                                            int usleep_microseconds,
                                            pid_t fork_pid)
{
    for (int i = 0; i < retries; ++i) {
        if (check_if_pidfile_process_is_running(pidfile)) {
            if (fork_pid > 0) {
                // reap now if the first child exited
                waitpid(fork_pid, nullptr, WNOHANG);
            }
            // small sleep to allow process to settle
            usleep(100000); // 100ms - small settling time
            return true;
        }

        // If we have a fork_pid, check whether that process died quickly
        if (fork_pid > 0) {
            int status = 0;
            pid_t r = waitpid(fork_pid, &status, WNOHANG);
            if (r == fork_pid) {
                // child exited - if it exited with non-zero, consider failure early
                DEBUG_LOG("[wait] fork_pid ", fork_pid, " exited while waiting for pidfile");
                break;
            }
        }

        usleep(usleep_microseconds);
    }

    // Final reap attempt
    if (fork_pid > 0) waitpid(fork_pid, nullptr, WNOHANG);
    return false;
}

bool
bk_mgmt::wait_for_pid_file_process_to_stop(const std::string &pidfile, int retries, int usleep_microseconds)
{
    for (int i = 0; i < retries; ++i) {
        if (!check_if_pidfile_process_is_running(pidfile)) return true;
        usleep(usleep_microseconds);
    }
    return false;
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

bool
bk_mgmt::kill_pidfile_process(const std::string &pidfile, int sig, int wait_retries, int wait_usleep)
{
    if (!bk_util::file_exists(pidfile)) return false;

    std::ifstream in(pidfile);
    pid_t pid;
    if (!(in >> pid)) return false;

    bool killed = false;

    // First attempt: send the desired signal
    if (kill(pid, sig) == 0) {
        for (int i = 0; i < wait_retries; ++i) {
            if (!check_if_pidfile_process_is_running(pidfile)) {
                killed = true;
                break;
            }
            usleep(wait_usleep);
        }
    }

    // Force kill if not stopped yet
    if (!killed) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        killed = true;
    }

    // Cleanup pidfile
    remove_pidfile_path (pidfile);
    return killed;
}