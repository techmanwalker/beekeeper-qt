#pragma once
#include <string>
#include <cstddef>
#include <cstdint>

namespace beekeeper { namespace privileged { namespace socketops {

// Connect to UNIX socket at path; returns fd or -1
int connect_unix_socket(int fd, const std::string& path, int timeout_ms);

// Listen on UNIX socket at path; returns fd or -1
int setup_unix_listener(const std::string &path, int backlog = 1);

// Write all bytes to fd
bool write_all(int fd, const void* buf, size_t len);

// Read exactly len bytes from fd
bool read_exact(int fd, void* buf, size_t len);

// Length-prefixed write
bool write_message(int fd, const std::string &msg);

// Length-prefixed read
bool read_message(int fd, std::string &out);
std::string read_message(int fd);

// Wait for a specific token string to appear on fd, with timeout (ms)
bool wait_for_ready_token(int fd, const std::string &token, int total_timeout_ms);

}}} // namespace
