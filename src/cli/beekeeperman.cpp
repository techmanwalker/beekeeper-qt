#include "../../include/beekeeper/beesdmgmt.hpp"
#include "../../include/beekeeper/btrfsetup.hpp"
#include "../../include/beekeeper/commandmachine.hpp"
#include "../../include/beekeeper/internalaliases.hpp" // required for bk_mgmt and bk_util aliases
#include "../../include/beekeeper/util.hpp"
#include <filesystem> // required for std::setw
#include <iostream>
#include <memory>

// Namespace alias
namespace cm = commandmachine;

// Command handler declarations
namespace beekeeper {
    // CLI handlers
    namespace cli {
        int handle_start(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_stop(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_restart(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_status(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_log(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_clean(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_help(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_setup(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_list(const std::map<std::string, std::string>&, const std::vector<std::string>&);
        int handle_stat(const std::map<std::string, std::string>&, const std::vector<std::string>&);
    }
}

// Command registry
std::vector<cm::Command> command_registry = {
    {
        "start", 
        beekeeper::cli::handle_start,
        {
            {"enable-logging", "l", false}  // long, short, requires_value
        },
        "UUID",
        "Start beesd daemon",
        1, -1
    },
    {
        "stop", 
        beekeeper::cli::handle_stop,
        {},
        "UUID",
        "Stop beesd daemon",
        1, -1
    },
    {
        "restart", 
        beekeeper::cli::handle_restart,
        {},
        "UUID",
        "Restart beesd daemon",
        1, -1
    },
    {
        "status", 
        beekeeper::cli::handle_status,
        {},
        "UUID",
        "Check beesd status",
        1, -1
    },
    {
        "log", 
        beekeeper::cli::handle_log,
        {},
        "UUID",
        "Show log file",
        1, -1
    },
    {
        "clean", 
        beekeeper::cli::handle_clean,
        {},
        "UUID",
        "Clean PID file",
        1, -1
    },
    {
        "setup",
        beekeeper::cli::handle_setup,
        {
            {"db-size", "d", true}  // Requires value (size in bytes)
        },
        "UUID",
        "Create/update configuration for a btrfs filesystem",
        1, 1
    },
    {
        "list",
        beekeeper::cli::handle_list,
        { {"json", "j", false} }, // <-- support -j / --json
        "",
        "List available btrfs filesystems",
        0, 0
    },
    {
        "stat",
        beekeeper::cli::handle_stat,
        {},
        "UUID",
        "Check if a btrfs filesystem has a configuration",
        1, 1
    },
    {
        "help", 
        beekeeper::cli::handle_help,
        {},
        "",
        "Show help information",
        0, 0
    }
};

// Helper: escape string for safe embedding into a JSON string value.
// Minimal escaping for JSON string values: backslash, quote and control chars.
static std::string
json_escape (const std::string &s)
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

// Command handler implementations
int
beekeeper::cli::handle_start(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects) {
    bool enable_logging = options.find("enable-logging") != options.end();
    
    for (const auto& uuid : subjects) {
        if (bk_mgmt::beesstart(uuid, enable_logging)) {
            std::cout << "Started beesd for " << uuid;
            if (enable_logging) {
                std::cout << " with logging enabled";
            }
            std::cout << std::endl;
        } else {
            std::cerr << "Failed to start beesd for " << uuid << std::endl;
            return 1;
        }
    }
    return 0;
}

int
beekeeper::cli::handle_stop(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects) {
    for (const auto& uuid : subjects) {
        if (bk_mgmt::beesstop(uuid)) {
            std::cout << "Stopped beesd for " << uuid << std::endl;
        } else {
            std::cerr << "Failed to stop beesd for " << uuid << std::endl;
            return 1;
        }
    }
    return 0;
}

int
beekeeper::cli::handle_restart(const std::map<std::string, std::string>& options, 
                   const std::vector<std::string>& subjects) {
    for (const auto& uuid : subjects) {
        if (bk_mgmt::beesrestart(uuid)) {
            std::cout << "Restarted beesd for " << uuid << std::endl;
        } else {
            std::cerr << "Failed to restart beesd for " << uuid << std::endl;
            return 1;
        }
    }
    return 0;
}

int
beekeeper::cli::handle_status(const std::map<std::string, std::string>& options, 
                  const std::vector<std::string>& subjects) {
    for (const auto& uuid : subjects) {
        std::cout << "Status for " << uuid << ": " << bk_mgmt::beesstatus(uuid) << std::endl;
    }
    return 0;
}

int
beekeeper::cli::handle_log(const std::map<std::string, std::string>& options, 
               const std::vector<std::string>& subjects) {
    bk_mgmt::beeslog(subjects[0]);
    return 0;
}

int
beekeeper::cli::handle_clean(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects) {
    bk_mgmt::beescleanlogfiles(subjects[0]);
    std::cout << "Cleaned PID file for " << subjects[0] << std::endl;
    return 0;
}

int
beekeeper::cli::handle_help(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects) {
    // Help is handled by the parser
    return 0;
}

int
beekeeper::cli::handle_setup(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects) 
{
    std::string uuid = subjects[0];
    size_t db_size = 0;
    
    // Parse db-size option if provided
    auto it = options.find("db-size");
    if (it != options.end()) {
        try {
            // FIXME: this code doesn't actually check if db_size is a positive integer
            db_size = std::stoull(it->second);
        } catch (...) {
            std::cerr << "Error: Invalid db-size value. Must be a positive integer.\n";
            return 1;
        }
    }
    
    std::string config_path = bk_mgmt::beessetup(uuid, db_size);
    if (!config_path.empty()) {
        std::cout << "Configuration created/updated: " << config_path << std::endl;
        return 0;
    } else {
        std::cerr << "Error: Failed to create/update configuration\n";
        return 1;
    }
}

int
beekeeper::cli::handle_list (const std::map<std::string, std::string>& options,
                             const std::vector<std::string>& subjects)
{
    auto filesystems = bk_mgmt::btrfsls();

    bool want_json = (options.find("json") != options.end());

    if (want_json) {
        // Emit compact JSON array (single-line) for machine consumption
        std::ostringstream out;
        out << '[';

        for (size_t i = 0; i < filesystems.size(); ++i) {
            const auto &fs = filesystems[i];

            // Safe access, 'uuid' is mandatory in btrfsls
            std::string uuid = (fs.find("uuid") != fs.end()) ? fs.at("uuid") : "";
            std::string label = (fs.find("label") != fs.end()) ? fs.at("label") : "";

            // Provide config path if any
            std::string config_path = bk_mgmt::btrfstat(uuid);

            out << '{';
            out << "\"uuid\":\""   << json_escape(uuid) << "\",";
            out << "\"label\":\""  << json_escape(label) << "\",";
            out << "\"config\":\"" << json_escape(config_path) << "\"";
            out << '}';

            if (i + 1 < filesystems.size()) out << ',';
        }

        out << ']';
        std::cout << out.str() << std::endl;
        return 0;
    }

    // -------------------------
    // Existing pretty-table path
    // -------------------------
    if (filesystems.empty()) {
        std::cout << "No btrfs filesystems found.\n";
        return 0;
    }

    // Precompute all status strings to determine max length
    std::vector<std::string> status_lines;
    size_t max_status_len = 15;  // "CONFIG STATUS" length

    for (const auto& fs : filesystems) {
        std::string config_path = bk_mgmt::btrfstat(fs.at("uuid"));
        std::string status = config_path.empty() ?
            "Not configured" :
            "Configured (" + config_path + ")";

        status_lines.push_back(status);
        max_status_len = std::max(max_status_len, status.length());
    }

    // Set column constraints
    const size_t MIN_UUID_LEN = 36;
    const size_t MAX_UUID_LEN = 60;
    const size_t MIN_LABEL_LEN = 5;
    const size_t MAX_LABEL_LEN = 40;
    const size_t MIN_STATUS_LEN = 15;
    const size_t MAX_STATUS_LEN = 80;

    // Calculate column widths
    size_t uuid_width = MIN_UUID_LEN;
    size_t label_width = MIN_LABEL_LEN;

    for (const auto& fs : filesystems) {
        uuid_width = std::max(uuid_width, fs.at("uuid").length());
        label_width = std::max(label_width, fs.at("label").length());
    }

    // Apply constraints
    uuid_width = std::min(std::max(uuid_width, MIN_UUID_LEN), MAX_UUID_LEN);
    label_width = std::min(std::max(label_width, MIN_LABEL_LEN), MAX_LABEL_LEN);
    max_status_len = std::min(std::max(max_status_len, MIN_STATUS_LEN), MAX_STATUS_LEN);

    // Table header with proper spacing
    std::cout << std::left
              << std::setw(uuid_width) << "UUID"
              << " "  // Space between columns
              << std::setw(label_width) << "LABEL"
              << " "  // Space between columns
              << "CONFIG STATUS"
              << "\n";

    // Separator line with proper spacing
    std::cout << std::string(uuid_width, '-')
              << " "  // Space between columns
              << std::string(label_width, '-')
              << " "  // Space between columns
              << std::string(max_status_len, '-')
              << "\n";

    // Print filesystems
    for (size_t i = 0; i < filesystems.size(); i++) {
        const auto& fs = filesystems[i];
        std::string uuid = fs.at("uuid");
        std::string label = fs.at("label");
        std::string status = status_lines[i];

        // Truncate with ellipsis if needed
        if (uuid.length() > uuid_width) {
            uuid = uuid.substr(0, uuid_width - 3) + "...";
        } else {
            uuid = uuid + std::string(uuid_width - uuid.length(), ' ');
        }

        if (label.length() > label_width) {
            label = label.substr(0, label_width - 3) + "...";
        } else {
            label = label + std::string(label_width - label.length(), ' ');
        }

        if (status.length() > max_status_len) {
            status = status.substr(0, max_status_len - 3) + "...";
        }

        // Print with proper spacing between columns
        std::cout << uuid
                  << " "  // Space between columns
                  << label
                  << " "  // Space between columns
                  << status
                  << "\n";
    }

    return 0;
}

int
beekeeper::cli::handle_stat(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects) 
{
    std::string uuid = subjects[0];
    std::string config_path = bk_mgmt::btrfstat(uuid);
    
    if (!config_path.empty()) {
        std::cout << "Configuration exists: " << config_path << std::endl;
        return 0;
    } else {
        std::cout << "No configuration found for " << uuid << std::endl;
        return 1;
    }
}

int
main(int argc, char* argv[]) {
    // beesd existence check
    if (!bk_util::command_exists("beesd")) {
        std::cerr << "Error: beesd is not installed\n";
        return 1;
    }

    // Create command parser
    auto parser = cm::CommandParser::create();
    
    // Parse and execute command
    return parser->parse(command_registry, argc, argv);
}