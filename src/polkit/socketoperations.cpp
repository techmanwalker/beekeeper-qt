#include "socketoperations.hpp"
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <vector>
#include <arpa/inet.h>
#include <chrono>
#include <cstring>

namespace beekeeper { namespace privileged { namespace socketops {

int connect_unix_socket(int fd, const std::string& path, int timeout_ms) {
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    std::memcpy(addr.sun_path, path.c_str(), path.size()+1);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return -1;
    }
    return fd;
}

int setup_unix_listener(const std::string &path, int backlog) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) { ::close(fd); errno = ENAMETOOLONG; return -1; }
    std::memcpy(addr.sun_path, path.c_str(), path.size()+1);
    ::unlink(path.c_str());

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { ::close(fd); return -1; }
    if (::listen(fd, backlog) < 0) { ::close(fd); return -1; }
    return fd;
}

bool write_all(int fd, const void* buf, size_t len) {
    const char* p = static_cast<const char*>(buf);
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::write(fd, p + total, len - total);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        total += n;
    }
    return true;
}

bool read_exact(int fd, void* buf, size_t len) {
    char* ptr = static_cast<char*>(buf);
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::read(fd, ptr + total, len - total);
        if (n > 0) total += n;
        else if (n == 0) return false;
        else if (errno != EINTR) return false;
    }
    return true;
}

bool write_message(int fd, const std::string &msg) {
    uint32_t len_be = htonl(msg.size());
    if (!write_all(fd, &len_be, sizeof(len_be))) return false;
    if (!msg.empty() && !write_all(fd, msg.data(), msg.size())) return false;
    return true;
}

bool read_message(int fd, std::string &out) {
    uint32_t len_be;
    if (!read_exact(fd, &len_be, sizeof(len_be))) return false;
    uint32_t len = ntohl(len_be);
    if (len == 0) { out.clear(); return true; }

    std::vector<char> buf(len);
    if (!read_exact(fd, buf.data(), len)) return false;
    out.assign(buf.data(), buf.size());
    return true;
}

std::string read_message(int fd) {
    std::string out;
    if (read_message(fd, out)) return out;
    return {};
}

bool wait_for_ready_token(int fd, const std::string &token, int total_timeout_ms) {
    std::string buf;
    buf.reserve(256);
    auto start = std::chrono::steady_clock::now();

    while (true) {
        struct pollfd pfd{fd, POLLIN, 0};
        int r = ::poll(&pfd, 1, 250);
        if (r > 0 && (pfd.revents & POLLIN)) {
            char tmp[256];
            ssize_t n = ::read(fd, tmp, sizeof(tmp));
            if (n > 0) {
                buf.append(tmp, n);
                if (buf.find(token) != std::string::npos) return true;
            } else if (n == 0) {
                return false; // EOF
            } else if (errno != EINTR) {
                return false;
            }
        } else if (r < 0 && errno != EINTR) {
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed_ms >= total_timeout_ms) return false;
    }
}

}}} // namespace
