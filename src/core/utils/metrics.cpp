#include "beekeeper/internalaliases.hpp"
#include "beekeeper/util.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

#ifdef __linux__
#include <fstream>
#endif

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

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

std::string
bk_util::current_timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_r(&now_time, &tm_now); // thread-safe localtime

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}