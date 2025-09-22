#include "beekeeper/util.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/poll.h>
#include <sys/wait.h>

/*
  Utility: drain two fds using poll until EOF on both,
  while also checking the child status via waitpid(WNOHANG).
*/
static void drain_pipes_and_reap(pid_t child,
                                 int fd_out,
                                 int fd_err,
                                 std::string &out, std::string &err)
{
    if (fd_out < 0 && fd_err < 0) {
        // nothing to do
        int status = 0;
        waitpid(child, &status, 0);
        return;
    }

    // non-blocking reads
    if (fd_out >= 0) fcntl(fd_out, F_SETFL, O_NONBLOCK);
    if (fd_err >= 0) fcntl(fd_err, F_SETFL, O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = fd_out;
    fds[0].events = (fd_out >= 0 ? POLLIN : 0);
    fds[1].fd = fd_err;
    fds[1].events = (fd_err >= 0 ? POLLIN : 0);

    int active = 0;
    if (fd_out >= 0) ++active;
    if (fd_err >= 0) ++active;

    std::array<char, 4096> buf;

    bool child_exited = false;
    while (active > 0 || !child_exited) {
        // poll with a reasonable timeout so we can check child status periodically
        int timeout_ms = 3000;
        int rv = poll(fds, 2, timeout_ms);
        if (rv < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (rv == 0) {
            // timeout — check child status
            int status = 0;
            pid_t r = waitpid(child, &status, WNOHANG);
            if (r == child) {
                child_exited = true;
                // if we've reaped child, continue to drain fds until EOF
            }
            // continue loop and poll again to drain remaining pipes
            continue;
        }

        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd < 0) continue;
            short re = fds[i].revents;
            if (re & (POLLIN | POLLHUP | POLLERR)) {
                ssize_t n = read(fds[i].fd, buf.data(), buf.size());
                if (n > 0) {
                    if (i == 0) out.append(buf.data(), n);
                    else err.append(buf.data(), n);
                } else if (n == 0) {
                    // EOF
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --active;
                } else { // n < 0
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // nothing right now
                    } else if (errno == EINTR) {
                        continue;
                    } else {
                        // unrecoverable read error: close and continue
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        --active;
                    }
                }
            }
        }

        // After draining any available data, check if child exited
        int status = 0;
        pid_t r = waitpid(child, &status, WNOHANG);
        if (r == child) child_exited = true;
    }

    // Ensure pipes closed
    if (fd_out >= 0) close(fd_out);
    if (fd_err >= 0) close(fd_err);

    // Wait for child if not reaped yet
    int status = 0;
    waitpid(child, &status, 0);
}

bool
bk_util::command_exists(const std::string& command)
{
    // Check if command is in PATH
    return !bk_util::which(command).empty();
}

void
bk_util::set_cloexec(int fd)
{
    if (fd < 0) return;
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) return;
    (void) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}


command_streams
bk_util::exec_command_shell(const char* cmd)
{
    command_streams result;
    if (!cmd) return result;

    int out_pipe[2] = {-1,-1};
    int err_pipe[2] = {-1,-1};

    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        std::cerr << "pipe() failed: " << strerror(errno) << std::endl;
        if (out_pipe[0] >= 0) close(out_pipe[0]);
        if (out_pipe[1] >= 0) close(out_pipe[1]);
        if (err_pipe[0] >= 0) close(err_pipe[0]);
        if (err_pipe[1] >= 0) close(err_pipe[1]);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed: " << strerror(errno) << std::endl;
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child: wire stdout/stderr to write ends
        close(out_pipe[0]);
        close(err_pipe[0]);

        // Duplicate the write ends to stdout/stderr
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        // Close originals
        close(out_pipe[1]);
        close(err_pipe[1]);

        // Exec shell
        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        // exec failed
        _exit(127);
    }

    // Parent: close write ends (we only read)
    close(out_pipe[1]);
    close(err_pipe[1]);

    // Drain and reap safely
    drain_pipes_and_reap(pid, out_pipe[0], err_pipe[0], result.stdout_str, result.stderr_str);

    return result;
}

// Execvp-based implementation using vector<string> -> argv
command_streams
bk_util::exec_commandv(const std::vector<std::string> &args)
{
    command_streams result;
    if (args.empty()) return result;

    int out_pipe[2] = {-1,-1};
    int err_pipe[2] = {-1,-1};

    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        std::cerr << "pipe() failed: " << strerror(errno) << std::endl;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed: " << strerror(errno) << std::endl;
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // child
        // Wire stdout/stderr
        close(out_pipe[0]);
        close(err_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        // Close unused
        close(out_pipe[1]);
        close(err_pipe[1]);

        // Build argv
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto &s : args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        // execvp
        execvp(argv[0], argv.data());
        _exit(127);
    }

    // parent: close the write ends we don't need
    close(out_pipe[1]);
    close(err_pipe[1]);

    // Drain and reap safely
    drain_pipes_and_reap(pid, out_pipe[0], err_pipe[0], result.stdout_str, result.stderr_str);

    return result;
}