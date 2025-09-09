#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "beekeeper/util.hpp"
#include "handlers.hpp"
#include <filesystem> // for std::setw
#include <iostream>

namespace fs = std::filesystem;

// Command handler implementations
int
beekeeper::cli::handle_start(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects)
{
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
                const std::vector<std::string>& subjects)
{
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
                   const std::vector<std::string>& subjects)
{
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
                  const std::vector<std::string>& subjects)
{
    for (const auto& uuid : subjects) {
        std::cout << "Status for " << uuid << ": " << bk_mgmt::beesstatus(uuid) << std::endl;
    }
    return 0;
}

int
beekeeper::cli::handle_log(const std::map<std::string, std::string>& options, 
               const std::vector<std::string>& subjects)
{
    bk_mgmt::beeslog(subjects[0]);
    return 0;
}

int
beekeeper::cli::handle_clean(const std::map<std::string, std::string>& options, 
                 const std::vector<std::string>& subjects)
{
    bk_mgmt::beescleanlogfiles(subjects[0]);
    std::cout << "Cleaned PID file for " << subjects[0] << std::endl;
    return 0;
}

int
beekeeper::cli::handle_help(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects)
{
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
            db_size = std::stoull(it->second);
            if (db_size == 0) {
                std::cerr << "Error: db-size must be a positive integer.\n";
                return 1;
            }
        } catch (...) {
            std::cerr << "Error: Invalid db-size value. Must be a positive integer.\n";
            return 1;
        }
    }

    // Check if "remove" option is set and has a non-empty value
    auto iu = options.find("remove");
    if (iu != options.end() && !iu->second.empty()) {
        std::string path = bk_mgmt::btrfstat(uuid);
        DEBUG_LOG("Removing config file: ", path);

        std::error_code ec;
        bool removed = fs::remove(path, ec);
        if (ec) {
            std::cerr << "Failed to remove " << path << ": " << ec.message() << "\n";
        } else if (!removed) {
            std::cerr << "Nothing removed (file did not exist): " << path << "\n";
        }
        return 0;
    }

    // Normal setup
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
beekeeper::cli::handle_locate(const std::map<std::string, std::string>& options,
                              const std::vector<std::string>& subjects)
{
    for (const auto &uuid : subjects) {
        // Call the privileged mount lookup directly
        std::string mountpoint = bk_mgmt::get_mount_path(uuid);

        if (!mountpoint.empty()) {
            std::cout << mountpoint << std::endl;
        } else {
            std::cerr << uuid << ": not mounted or not found" << std::endl;
        }
    }

    return 0;
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
            out << "\"uuid\":\""   << bk_util::json_escape(uuid) << "\",";
            out << "\"label\":\""  << bk_util::json_escape(label) << "\",";
            out << "\"config\":\"" << bk_util::json_escape(config_path) << "\"";
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

    // Check if storage option was passed
    auto it_storage = options.find("storage");
    if (it_storage != options.end()) {
        std::string mode = it_storage->second;

        if (mode == "free") {
            std::cout << bk_mgmt::get_space::free(uuid) << std::endl;
            return 0;
        } else if (mode == "used") {
            std::cout << bk_mgmt::get_space::used(uuid) << std::endl;
            return 0;
        } else {
            // Any other string (or empty) → print both
            auto free_val = bk_mgmt::get_space::free(uuid);
            auto used_val = bk_mgmt::get_space::used(uuid);

            auto it_json = options.find("json");
            if (it_json != options.end() && !it_json->second.empty()) {
                // JSON output
                std::cout << "{\"free\": " << free_val
                          << ", \"used\": " << used_val << "}" << std::endl;
            } else {
                // Plain output
                std::cout << "Free space: " << free_val << std::endl;
                std::cout << "Used space: " << used_val << std::endl;
            }
            return 0;
        }
    }

    // Default path: check configuration only
    std::string config_path = bk_mgmt::btrfstat(uuid);
    if (!config_path.empty()) {
        std::cout << "Configuration exists: " << config_path << std::endl;
        return 0;
    } else {
        std::cout << "No configuration found for " << uuid << std::endl;
        return 1;
    }
}