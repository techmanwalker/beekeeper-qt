#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "beekeeper/util.hpp"
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <unistd.h> 
#include <vector>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __linux__
#include <fstream>
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif


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

// Execvp-based implementation using vector<string> -> argv
command_streams
bk_util::exec_commandv(const std::vector<std::string> &args)
{
    command_streams result;

    if (args.empty()) return result;

    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };

    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        std::cerr << "pipe() failed: " << strerror(errno) << std::endl;
        return result;
    }

    set_cloexec(out_pipe[0]);
    set_cloexec(out_pipe[1]);
    set_cloexec(err_pipe[0]);
    set_cloexec(err_pipe[1]);

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "fork() failed: " << strerror(errno) << std::endl;
        return result;
    }

    if (pid == 0) { // child
        // Redirect stdout/stderr
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        // Close unused in child
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);

        // Build argv
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto &s : args) {
            argv.push_back(const_cast<char*>(s.c_str()));
        }
        argv.push_back(nullptr);

        // execvp uses PATH
        execvp(argv[0], argv.data());
        // If exec fails:
        _exit(127);
    }

    // parent
    close(out_pipe[1]);
    close(err_pipe[1]);

    // Non-blocking read setup
    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = out_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = err_pipe[0];
    fds[1].events = POLLIN;

    std::array<char, 4096> buf;
    int active_fds = 2;
    while (active_fds > 0) {
        int rv = poll(fds, 2, 5000);
        if (rv < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < 2; ++i) {
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                ssize_t n = read(fds[i].fd, buf.data(), buf.size());
                if (n > 0) {
                    if (i == 0)
                        result.stdout_str.append(buf.data(), n);
                    else
                        result.stderr_str.append(buf.data(), n);
                } else if (n == 0) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --active_fds;
                } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    --active_fds;
                }
            }
        }
    }

    if (out_pipe[0] >= 0) close(out_pipe[0]);
    if (err_pipe[0] >= 0) close(err_pipe[0]);

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
bk_util::which(const std::string &program)
{
    std::string cmd = bk_util::trim_string(program);

    // Take only the first word up to the first space
    size_t space_pos = cmd.find(' ');
    if (space_pos != std::string::npos) {
        cmd = cmd.substr(0, space_pos);
    }

    const char *pathEnv = std::getenv("PATH");
    if (!pathEnv) return "";

    std::stringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        std::string candidate = dir + "/" + cmd;
        if (::access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }

    return "";
}

// Helper: escape string for safe embedding into a JSON string value.
// Minimal escaping for JSON string values: backslash, quote and control chars.
std::string
bk_util::json_escape (const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    // control character -> \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

std::string
bk_util::trip_quotes(const std::string &s)
{
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string
bk_util::quote_if_needed(const std::string &input)
{
    if (input.empty())
        return "\"\"";

    if (input.front() == '"' && input.back() == '"')
        return input;

    return "\"" + input + "\"";
}

// Divide and apply suffix to a byte size number
std::string
bk_util::auto_size_suffix(size_t size_in_bytes)
{
    double size = static_cast<double>(size_in_bytes);
    std::vector<std::string> suffixes = {"", "KiB", "MiB", "GiB", "TiB", "PiB"};

    size_t i = 0;
    while (i + 1 < suffixes.size() && size >= 1024.0) {
        size /= 1024.0;
        ++i;
    }

    // Round to 2 decimal places
    std::ostringstream oss;
    if (std::fabs(size - std::round(size)) < 1e-9) {
        // Integer, drop decimals
        oss << static_cast<int64_t>(std::round(size));
    } else {
        // Up to 2 decimals
        oss << std::fixed << std::setprecision(2) << size;
    }

    if (!suffixes[i].empty()) {
        oss << " " << suffixes[i];
    }

    return oss.str();
}

// Trim helper: remove everything up to and including the first ':' and trim whitespace
std::string
bk_util::trim_config_path_after_colon(const std::string &raw)
{
    if (raw.empty())
        return "";

    // special-case beekeeperman "no config" message
    if (raw.rfind("No configuration found", 0) == 0)
        return "";

    std::string s = raw;
    auto pos = s.find(':');
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }
    // trim leading
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    // trim trailing
    if (!s.empty()) {
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
    }
    return s;
}

std::string
bk_util::serialize_vector(const std::vector<std::string> &vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << vec[i];
        if (i + 1 < vec.size())
            oss << ", ";
    }
    oss << "]";
    return oss.str();
}

// Autostart helpers

bool
bk_util::is_uuid(const std::string &s)
{
    static const std::regex uuid_pattern(
        R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})"
    );
    return std::regex_match(s, uuid_pattern);
}

std::vector<std::string>
bk_util::list_uuids_in_autostart(const std::string &cfg_file)
{
    std::vector<std::string> uuids;
    std::ifstream file(cfg_file);
    if (!file.is_open())
        return uuids; // file does not exist

    std::string line;
    while (std::getline(file, line))
    {
        // trim whitespace front/back
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        if (!line.empty())
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty())
            continue;

        if (is_uuid(line))
            uuids.push_back(line);
    }

    return uuids;
}

bool
bk_util::is_uuid_in_autostart(const std::string &uuid)
{
    std::vector<std::string> uuids = list_uuids_in_autostart();

    // Trim the input UUID just in case
    std::string trimmed_uuid = trim_string(uuid);

    // Si el vector está vacío, devuelve false inmediatamente
    if (uuids.empty())
        return false;

    for (auto &u : uuids) {
        if (trim_string(u) == trimmed_uuid)
            return true;
    }

    return false;
}

std::string
bk_util::get_second_token (std::string line)
{
    // Shove off the first token
    size_t start = line.find_first_of(" \t");
    if (start == std::string::npos) {
        return "";  // Empty line
    }
    line = line.substr(start);

    // Trim whitespace
    line = bk_util::trim_string(line);
    
    // Find first whitespace after PID to shove off everything else
    size_t end = line.find_first_of(" \t", start);
    if (end == std::string::npos) {
        end = line.length();
    }
    std::string token = line.substr(0, end);

    return token;
}

double
bk_util::current_cpu_usage(int decimals)
{
#ifdef __linux__
    static std::vector<unsigned long long> last_total;
    static std::vector<unsigned long long> last_idle;

    std::ifstream file("/proc/stat");
    if (!file.is_open()) return -1.0;

    std::vector<unsigned long long> total;
    std::vector<unsigned long long> idle;

    std::string line;
    while (std::getline(file, line)) {
        if (line.substr(0, 3) != "cpu") break; // stop after cpu lines
        std::istringstream ss(line);
        std::string cpu;
        ss >> cpu;

        unsigned long long user, nice, system, idle_time, iowait, irq, softirq, steal;
        ss >> user >> nice >> system >> idle_time >> iowait >> irq >> softirq >> steal;

        unsigned long long tot = user + nice + system + idle_time + iowait + irq + softirq + steal;
        total.push_back(tot);
        idle.push_back(idle_time + iowait);
    }

    double usage = 0.0;
    for (size_t i = 1; i < total.size(); ++i) { // skip total (0)
        unsigned long long d_total = total[i] - (i < last_total.size() ? last_total[i] : 0);
        unsigned long long d_idle  = idle[i]  - (i < last_idle.size()  ? last_idle[i]  : 0);
        if (d_total > 0) usage += 100.0 * (d_total - d_idle) / d_total;
    }

    last_total = total;
    last_idle  = idle;

    // Average per core
    if (total.size() > 1) usage /= (total.size() - 1);

    double factor = std::pow(10.0, decimals);
    return std::round(usage * factor) / factor;

#elif defined(__FreeBSD__)
    long cp_time[CPUSTATES];
    size_t len = sizeof(cp_time);

    if (sysctlbyname("kern.cp_time", &cp_time, &len, nullptr, 0) == -1) return -1.0;

    unsigned long long total = 0;
    for (int i = 0; i < CPUSTATES; ++i) total += cp_time[i];

    unsigned long long idle = cp_time[CP_IDLE] + cp_time[CP_IDLE]; // conservative
    double usage = 100.0 * (total - idle) / total;

    double factor = std::pow(10.0, decimals);
    return std::round(usage * factor) / factor;

#else
    // Fallback: not supported
    return -1.0;
#endif
}