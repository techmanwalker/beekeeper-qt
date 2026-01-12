#pragma once
#ifdef BEEKEEPER_DEBUG_LOGGING
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <type_traits>
#include <utility>

// Runtime debug logging control
class DebugLogger {
public:
    static bool enabled() {
        static bool enabled = [](){
            const char* env = std::getenv("BEEKEEPER_DEBUG");
            if (env && env[0] == '1') {
                std::cerr << "Debug logging enabled via environment variable" << std::endl;
                return true;
            }
            
            #if defined(BEEKEEPER_DEBUG_LOGGING)
                std::cerr << "Debug logging enabled via compile flag" << std::endl;
                return true;
            #endif
            
            return false;
        }();
        return enabled;
    }
};

// Default debug_print implementation
template<typename T>
void debug_print(std::ostream& os, const T& value) {
    os << value;
}

// Base case for recursive printing
inline void _debug_print_helper(std::ostream&) {}

// Recursive variadic template
template<typename T, typename... Args>
void _debug_print_helper(std::ostream& os, T&& first, Args&&... rest) {
    debug_print(os, std::forward<T>(first));
    if constexpr (sizeof...(rest) > 0) {
        _debug_print_helper(os, std::forward<Args>(rest)...);
    }
}

// Global log file accessor
inline std::ofstream& debug_log_file() {
    static std::ofstream ofs;
    static bool init = false;
    if (!init) {
        ofs.open("/tmp/beekeeper-debug.log", std::ios::out | std::ios::app);
        init = true;
    }
    return ofs;
}

// Compile-time path stripper
constexpr const char* shorten_path(const char* path) {
    const char* last_slash = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/') last_slash = p + 1;
    }
    return last_slash;
}

// We can also strip up to "src/" instead of just the last slash
constexpr const char* strip_to_repo(const char* path) {
    const char* repo_marker = nullptr;
    for (const char* p = path; *p; ++p) {
        if (p[0] == 's' && p[1] == 'r' && p[2] == 'c' && p[3] == '/') {
            repo_marker = p;
        }
    }
    return repo_marker ? repo_marker : shorten_path(path);
}

#define __SHORT_FILE__ strip_to_repo(__FILE__)

// Overloads

// Handle C string literals
inline void debug_print(std::ostream& os, const char* value) {
    os << value;
}

// Handle integers (any integral type)
template<typename T>
inline std::enable_if_t<std::is_integral_v<T>, void>
debug_print(std::ostream& os, T value) {
    os << value;
}

// std::thread::id
inline void debug_print(std::ostream& os, const std::thread::id& tid) {
    std::ostringstream oss;
    oss << tid;
    os << oss.str();
}

// Debug logging macro
#define DEBUG_LOG(...) do { \
    if (DebugLogger::enabled()) { \
        std::ostringstream debug_oss; \
        debug_oss << "[" << __SHORT_FILE__ << ":" << __LINE__ << "] "; \
        _debug_print_helper(debug_oss, __VA_ARGS__); \
        std::string msg = debug_oss.str(); \
        std::cerr << msg << std::endl; \
        debug_log_file() << msg << std::endl; \
    } \
} while(0) // <- NO backslash here, file ends naturally

#else // !BEEKEEPER_DEBUG_LOGGING

// No-op in release
#define DEBUG_LOG(...) do { } while(0)

#endif