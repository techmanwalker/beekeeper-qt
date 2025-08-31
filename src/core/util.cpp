#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "beekeeper/util.hpp"
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h> 
#include <sys/stat.h>
#include <sys/wait.h>

// Simple case-insensitive character comparison
bool
bk_util::char_equal_ignore_case (char a, char b)
{
    return std::tolower(static_cast<unsigned char>(a)) == 
           std::tolower(static_cast<unsigned char>(b));
}

// Case-insensitive string comparison
bool
bk_util::string_equal_ignore_case (const std::string& a, const std::string& b)
{
    if (a.length() != b.length()) return false;
    
    for (size_t i = 0; i < a.length(); ++i) {
        if (!bk_util::char_equal_ignore_case(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

bool
bk_util::command_exists(const std::string& command)
{
    // Check if command is in PATH
    std::string test_cmd = "command -v " + command + " >/dev/null 2>&1";
    int status = std::system(test_cmd.c_str());
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

command_streams
bk_util::exec_command(const char* cmd)
{
    command_streams result;

    int out_pipe[2];
    int err_pipe[2];

    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        std::cerr << "pipe() failed: " << strerror(errno) << std::endl;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed: " << strerror(errno) << std::endl;
        return result;
    }

    if (pid == 0) { // Child
        close(out_pipe[0]);
        close(err_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[1]);
        close(err_pipe[1]);

        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        _exit(127); // exec failed
    }

    // Parent
    close(out_pipe[1]);
    close(err_pipe[1]);

    std::array<char, 256> buffer;
    ssize_t n;

    // Read stdout
    while ((n = read(out_pipe[0], buffer.data(), buffer.size())) > 0) {
        result.stdout_str.append(buffer.data(), n);
    }
    close(out_pipe[0]);

    // Read stderr
    while ((n = read(err_pipe[0], buffer.data(), buffer.size())) > 0) {
        result.stderr_str.append(buffer.data(), n);
    }
    close(err_pipe[0]);

    DEBUG_LOG("[beekeeper-helper] stdout len=", result.stdout_str.size(), " text: ", result.stdout_str);
    DEBUG_LOG("[beekeeper-helper] stderr len=", result.stderr_str.size(), " text: ", result.stderr_str);

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        std::cerr << "Command exited with code: " << WEXITSTATUS(status) << std::endl;
    }

    return result;
}

bool
bk_util::file_exists (const std::string& path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool
bk_util::file_readable (const std::string& path)
{
    if (access(path.c_str(), R_OK) != 0) {
        std::cerr << "File access error (" << path << "): " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool
bk_util::is_root ()
{
    return geteuid() == 0;
}

std::string
bk_util::trim_string (const std::string& str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return std::string(start, end + 1);
}

std::string
bk_util::to_lower(const std::string& str) {
    std::string lower;
    for (char c : str) {
        lower += std::tolower(static_cast<unsigned char>(c));
    }
    return lower;
}

std::string
bk_util::which(const std::string &program) {
    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";

    std::stringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        std::string candidate = dir + "/" + program;
        if (::access(candidate.c_str(), X_OK) == 0) {
            return candidate; // found
        }
    }
    return ""; // not found
}