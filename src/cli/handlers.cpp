#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "beekeeper/util.hpp"
#include "commandmachine/parser.hpp"
#include "commandregistry.hpp"
#include "handlers.hpp"
#include <filesystem> // for std::setw
#include <iostream>
#include <string>

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
    bool json_mode = options.count("json") > 0;

    auto emit_json = [&](int success, const std::string &message) {
        std::cout 
            << "{\n"
            << "  \"success\": " << success << ",\n"
            << "  \"message\": \"" << message << "\"\n"
            << "}\n";
    };

    std::string uuid = subjects.empty() ? "" : subjects[0];
    size_t db_size = 0;

    auto it = options.find("db-size");
    if (it != options.end()) {
        try {
            db_size = std::stoull(it->second);
            if (db_size == 0) {
                if (json_mode) emit_json(0, "Error: db-size must be a positive integer.");
                else std::cerr << "Error: db-size must be a positive integer.\n";
                return 1;
            }
        } catch (...) {
            if (json_mode) emit_json(0, "Error: Invalid db-size value. Must be a positive integer.");
            else std::cerr << "Error: Invalid db-size value. Must be a positive integer.\n";
            return 1;
        }
    }

    // Remove path
    auto iu = options.find("remove");
    if (iu != options.end() && !iu->second.empty()) {
        std::string path = bk_mgmt::btrfstat(uuid);
        std::error_code ec;
        bool removed = fs::remove(path, ec);
        if (ec) {
            if (json_mode) emit_json(0, "Failed to remove " + path + ": " + ec.message());
            else std::cerr << "Failed to remove " << path << ": " << ec.message() << "\n";
        } else if (!removed) {
            if (json_mode) emit_json(0, "Nothing removed (file did not exist): " + path);
            else std::cerr << "Nothing removed (file did not exist): " << path << "\n";
        } else {
            if (json_mode) emit_json(1, "Removed config: " + path);
            else std::cout << "Removed config: " << path << "\n";
        }
        return 0;
    }

    // Normal setup
    std::string config_path = bk_mgmt::beessetup(uuid, db_size);
    if (!config_path.empty()) {
        if (json_mode) emit_json(1, "Configuration created/updated: " + config_path);
        else std::cout << "Configuration created/updated: " << config_path << "\n";
        return 0;
    } else {
        if (json_mode) emit_json(0, "Error: Failed to create/update configuration");
        else std::cerr << "Error: Failed to create/update configuration\n";
        return 1;
    }
}

int
beekeeper::cli::handle_locate(const std::map<std::string, std::string>& options,
                              const std::vector<std::string>& subjects)
{
    bool json_output = options.find("json") != options.end();

    if (json_output) {
        // --- JSON output ---
        std::cout << "{";

        bool first_uuid = true;
        for (const auto &uuid : subjects) {
            std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);

            if (!first_uuid) {
                std::cout << ", ";
            }
            first_uuid = false;

            std::cout << "\"" << uuid << "\": [";

            bool first_mp = true;
            for (const auto &mp : mountpoints) {
                if (!first_mp) {
                    std::cout << ", ";
                }
                first_mp = false;
                std::cout << "\"" << mp << "\"";
            }

            std::cout << "]";
        }

        std::cout << "}" << std::endl;
    } else {
        // --- Pretty-printed human output ---
        for (const auto &uuid : subjects) {
            std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);

            if (!mountpoints.empty()) {
                std::cout << "Points that " << uuid << " is mounted on:" << std::endl;
                for (const auto &mp : mountpoints) {
                    std::cout << "\t" << mp << std::endl;
                }
            } else {
                std::cerr << uuid << ": not mounted or not found" << std::endl;
            }
        }
    }

    return 0;
}

int
beekeeper::cli::handle_list(const std::map<std::string, std::string>& options,
                            const std::vector<std::string>& subjects)
{
    auto filesystems = bk_mgmt::btrfsls();

    bool want_json = (options.find("json") != options.end());

    if (want_json) {
        // Emit compact JSON array for machine consumption
        std::ostringstream out;
        out << '[';

        for (size_t i = 0; i < filesystems.size(); ++i) {
            const auto &fs = filesystems[i];

            std::string uuid    = (fs.find("uuid")    != fs.end()) ? fs.at("uuid")    : "";
            std::string label   = (fs.find("label")   != fs.end()) ? fs.at("label")   : "";
            std::string status  = (fs.find("status")  != fs.end()) ? fs.at("status")  : "";
            std::string devname = (fs.find("devname") != fs.end()) ? fs.at("devname") : "";

            // Provide config path (always included in JSON)
            std::string config_path = bk_mgmt::btrfstat(uuid);

            out << '{';
            out << "\"uuid\":\""    << bk_util::json_escape(uuid)    << "\",";
            out << "\"label\":\""   << bk_util::json_escape(label)   << "\",";
            out << "\"status\":\""  << bk_util::json_escape(status)  << "\",";
            out << "\"devname\":\"" << bk_util::json_escape(devname) << "\",";
            out << "\"config\":\""  << bk_util::json_escape(config_path) << "\"";
            out << '}';

            if (i + 1 < filesystems.size()) out << ',';
        }

        out << ']';
        std::cout << out.str() << std::endl;
        return 0;
    }

    // -------------------------
    // Pretty-table (human readable)
    // -------------------------
    if (filesystems.empty()) {
        std::cout << "No btrfs filesystems found.\n";
        return 0;
    }

    // Precompute all status strings
    std::vector<std::string> status_lines;
    size_t max_status_len = 6; // "STATUS" header length

    for (const auto &fs : filesystems) {
        std::string status = (fs.find("status") != fs.end()) ? fs.at("status") : "unknown";
        status_lines.push_back(status);
        max_status_len = std::max(max_status_len, status.length());
    }

    // Column constraints
    constexpr size_t MIN_UUID_LEN   = 9;
    constexpr size_t MAX_UUID_LEN   = 36;
    constexpr size_t MIN_LABEL_LEN  = 5;
    constexpr size_t MAX_LABEL_LEN  = 40;
    constexpr size_t MIN_STATUS_LEN = 6;
    constexpr size_t MAX_STATUS_LEN = 30;

    // Calculate column widths
    size_t uuid_width  = MIN_UUID_LEN;
    size_t label_width = MIN_LABEL_LEN;

    for (const auto &fs : filesystems) {
        uuid_width  = std::max(uuid_width,  fs.at("uuid").length());
        label_width = std::max(label_width, fs.at("label").length());
    }

    // Apply constraints
    uuid_width  = std::min(std::max(uuid_width,  MIN_UUID_LEN),  MAX_UUID_LEN);
    label_width = std::min(std::max(label_width, MIN_LABEL_LEN), MAX_LABEL_LEN);
    max_status_len = std::min(std::max(max_status_len, MIN_STATUS_LEN), MAX_STATUS_LEN);

    // Table header
    std::cout << std::left
              << std::setw(uuid_width)  << "UUID"   << " "
              << std::setw(label_width) << "LABEL"  << " "
              << std::setw(max_status_len) << "STATUS"
              << "\n";

    // Separator line
    std::cout << std::string(uuid_width, '-')  << " "
              << std::string(label_width, '-') << " "
              << std::string(max_status_len, '-') << "\n";

    // Print rows
    for (size_t i = 0; i < filesystems.size(); i++) {
        const auto& fs = filesystems[i];
        std::string uuid   = fs.at("uuid");
        std::string label  = fs.at("label");
        std::string status = status_lines[i];

        if (uuid.length() > uuid_width) {
            uuid = uuid.substr(0, uuid_width - 3) + "...";
        } else {
            uuid += std::string(uuid_width - uuid.length(), ' ');
        }

        if (label.length() > label_width) {
            label = label.substr(0, label_width - 3) + "...";
        } else {
            label += std::string(label_width - label.length(), ' ');
        }

        if (status.length() > max_status_len) {
            status = status.substr(0, max_status_len - 3) + "...";
        }

        std::cout << uuid << " "
                  << label << " "
                  << status
                  << "\n";
    }

    return 0;
}

int
beekeeper::cli::handle_stat(const std::map<std::string, std::string>& options,
                            const std::vector<std::string>& subjects)
{
    if (subjects.empty()) {
        std::cerr << "Error: UUID not specified" << std::endl;
        return 1;
    }

    auto emit_json = [&](bool success, const std::string &key, const std::string &val) {
        std::cout << "{\n"
                  << "  \"success\": " << (success ? 1 : 0) << ",\n"
                  << "  \"" << key << "\": \"" << val << "\"\n"
                  << "}\n";
    };

    std::string uuid = subjects[0];
    std::string mode;
    bool json = false;

    if (auto it = options.find("storage"); it != options.end()) {
        mode = it->second;
    }
    if (auto it = options.find("json"); it != options.end()) {
        json = true;
    }

    // Storage reporting
    if (!mode.empty()) {
        int64_t free_val = bk_mgmt::get_space::free(uuid);
        int64_t used_val = bk_mgmt::get_space::used(uuid);

        if (mode == "free") {
            if (json) emit_json(true, "free", std::to_string(free_val));
            else std::cout << bk_util::auto_size_suffix(free_val) << std::endl;
            return 0;
        } else if (mode == "used") {
            if (json) emit_json(true, "used", std::to_string(used_val));
            else std::cout << bk_util::auto_size_suffix(used_val) << std::endl;
            return 0;
        } else {
            if (json) {
                std::cout << "{\n"
                          << "  \"success\": 1,\n"
                          << "  \"free\": " << free_val << ",\n"
                          << "  \"used\": " << used_val << "\n"
                          << "}\n";
            } else {
                std::cout << "Free space: " << bk_util::auto_size_suffix(free_val) << std::endl;
                std::cout << "Used space: " << bk_util::auto_size_suffix(used_val) << std::endl;
            }
            return 0;
        }
    }

    // Config check
    std::string config_path = bk_mgmt::btrfstat(uuid);
    if (!config_path.empty()) {
        if (json) emit_json(true, "config_path", config_path);
        else std::cout << "Configuration exists: " << config_path << std::endl;
        return 0;
    } else {
        if (json) emit_json(false, "config_path", "");
        else std::cout << "No configuration found for " << uuid << std::endl;
        return 1;
    }
}

int
beekeeper::cli::handle_autostartctl(const std::map<std::string, std::string> &options,
                                    const std::vector<std::string> &subjects)
{
    bool add = options.find("add") != options.end();
    bool remove = options.find("remove") != options.end();

    if (add && remove) {
        commandmachine::command_parser_impl parser;
        parser.print_help(command_registry);
        return 1;
    }

    for (const std::string &uuid_str : subjects) {
        if (add)
            bk_mgmt::autostart::add_uuid(uuid_str);
        else if (remove)
            bk_mgmt::autostart::remove_uuid(uuid_str);
    }

    return 0;
}

int
beekeeper::cli::handle_compressctl(const std::map<std::string, std::string> &options,
                                   const std::vector<std::string> &subjects)
{
    namespace tc = bk_mgmt::transparentcompression;

    bool start  = options.find("start")  != options.end() || options.find("s") != options.end();
    bool pause  = options.find("pause")  != options.end() || options.find("p") != options.end();
    bool status = options.find("status") != options.end() || options.find("i") != options.end();
    bool add    = options.find("add")    != options.end() || options.find("a") != options.end();
    bool remove = options.find("remove") != options.end() || options.find("r") != options.end();

    bool want_json = (options.find("json") != options.end()) || (options.find("j") != options.end());

    // Prevent conflicting options: only one action allowed at a time
    int chosen = (start ? 1 : 0) + (pause ? 1 : 0) + (status ? 1 : 0) + (add ? 1 : 0) + (remove ? 1 : 0);
    if (chosen != 1) {
        commandmachine::command_parser_impl parser;
        parser.print_help(command_registry);
        return 1;
    }

    std::string algo;
    int level = 0;

    if (add) {
        // Preset mapping
        static const std::unordered_map<std::string, std::pair<std::string, int>> preset_map = {
            {"feather",     {"lzo", 0}},
            {"light",       {"zstd", 1}},
            {"balanced",    {"zstd", 3}},
            {"high",        {"zstd", 6}},
            {"harder",      {"zstd", 10}},
            {"maximum",     {"zstd", 15}}
        };

        // 1) --compression-level / -c
        auto it_comp = options.find("compression-level");

        if (it_comp != options.end()) {
            std::string preset = bk_util::to_lower(it_comp->second);
            if (preset == "<default>") preset = "";          // normalize <default>
            auto it = preset_map.find(preset);
            if (it != preset_map.end()) {
                algo = it->second.first;
                level = it->second.second;
            } else if (!preset.empty()) {                    // unknown preset, warn
                DEBUG_LOG("[compressctl] unknown compression-level preset: ", preset);
                algo = "lzo";
                level = 0;
            }
        }

        // 2) Override with --algorithm / --algo
        auto it_algo = options.find("algorithm");
        if (it_algo == options.end())
            it_algo = options.find("algo");

        if (it_algo != options.end()) {
            algo = bk_util::to_lower(it_algo->second);
            if (algo == "<default>") algo.clear();           // normalize <default>
        }

        // 3) Override with --level
        auto it_level = options.find("level");
        if (it_level != options.end()) {
            std::string lvl_str = it_level->second;
            if (lvl_str == "<default>") {
                level = 0;                                   // normalize <default>
            } else {
                try {
                    level = std::stoi(lvl_str);
                } catch (...) {
                    DEBUG_LOG("[compressctl] invalid level, defaulting to 0");
                    level = 0;
                }
            }
        }

        // 4) Fallback default
        if (algo.empty())
            algo = "lzo";
    }

    if (status && want_json) {
        std::cout << "[";
    }

    bool first_json_item = true;

    for (const std::string &uuid_str : subjects) {
        if (start) {
            DEBUG_LOG("[compressctl] start compression for UUID ", uuid_str);
            tc::start(uuid_str);
        } else if (pause) {
            DEBUG_LOG("[compressctl] pause compression for UUID ", uuid_str);
            tc::pause(uuid_str);
        } else if (status) {
            DEBUG_LOG("[compressctl] query status for UUID ", uuid_str);

            bool enabled = tc::is_enabled_for(uuid_str);
            bool running = tc::is_running(uuid_str);

            // Get algorithm + level currently active
            auto [algorithm, level_str] = bk_mgmt::transparentcompression::get_current_compression_level(uuid_str);

            if (want_json) {
                if (!first_json_item) std::cout << ",";
                first_json_item = false;

                std::cout << "\n  {"
                          << "\"uuid\":\"" << uuid_str << "\","
                          << "\"enabled\":" << (enabled ? "true" : "false") << ","
                          << "\"running\":" << (running ? "true" : "false") << ","
                          << "\"algorithm\":\"" << algorithm << "\","
                          << "\"level\":\"" << level_str << "\""
                          << "}";
            } else {
                std::cout << uuid_str << ": "
                          << (enabled ? "Enabled to automatically compress at boot; "
                                      : "Disabled to automatically compress at boot; ")
                          << (running ? "compressing" : "paused, not running");

                if (!algorithm.empty() && algorithm != "none") {
                    std::cout << " with algorithm " << algorithm;
                    if (!level_str.empty() && level_str != "0") {
                        std::cout << " at level " << level_str;
                    }
                }

                std::cout << std::endl;
            }
        } else if (add) {
            DEBUG_LOG("[compressctl] add compression config for UUID ", uuid_str,
                      " algo=", algo, " level=", level);
            tc::add_uuid(uuid_str, algo, level);
        } else if (remove) {
            DEBUG_LOG("[compressctl] remove compression config for UUID ", uuid_str);
            tc::remove_uuid(uuid_str);
        }
    }

    if (status && want_json) {
        if (!first_json_item) std::cout << "\n";
        std::cout << "]" << std::endl;
    }

    return 0;
}
