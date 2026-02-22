#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "beekeeper/util.hpp"
#include "bk-clauses.hpp"
#include <filesystem> // for std::setw
#include <string>
#include <sstream>

namespace fs = std::filesystem;
namespace clauses = beekeeper::clauses;

// Ease early returns

#define RETURN_COMMANDSTREAMS \
    return command_streams { \
        cout.str(), \
        cerr.str(), \
        errcode \
    };

// Clause handler implementations
command_streams
clauses::start(const clause_options &options, 
               const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;

    bool enable_logging = options.find("enable-logging") != options.end();
    
    for (const auto& uuid : subjects) {
        if (bk_mgmt::beesstart(uuid)) {
            cout << "Started beesd for " << uuid;
            if (enable_logging) {
                cout << " with logging enabled";
            }
            cout << std::endl;
        } else {
            cerr << "Failed to start beesd for " << uuid << std::endl;
            errcode = 1;
        }
    }

    RETURN_COMMANDSTREAMS
}

command_streams
clauses::stop(const clause_options &options, 
              const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;

    for (const auto& uuid : subjects) {
        if (bk_mgmt::beesstop(uuid)) {
            cout << "Stopped beesd for " << uuid << std::endl;
        } else {
            cerr << "Failed to stop beesd for " << uuid << std::endl;
            errcode = 1;
        }
    }
    
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::restart(const clause_options &options, 
                 const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;

    for (const auto& uuid : subjects) {
        if (bk_mgmt::beesrestart(uuid)) {
            cout << "Restarted beesd for " << uuid << std::endl;
        } else {
            cerr << "Failed to restart beesd for " << uuid << std::endl;
            errcode = 1;
        }
    }
    
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::status(const clause_options &options, 
                const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;

    for (const auto& uuid : subjects) {
        cout << "Status for " << uuid << ": " << bk_mgmt::beesstatus(uuid) << std::endl;
    }
    
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::log(const clause_options &options, 
             const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    
    // Deactivated, may remove
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::clean(const clause_options &options, 
               const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;

    if (subjects.empty()) {
        cerr << "No UUID specified";
        errcode = 1;
        RETURN_COMMANDSTREAMS
    }
    
    bk_mgmt::beescleanlogfiles(subjects[0]);

    cout << "Cleaned PID file for " << subjects[0];
    
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::help(const clause_options &options, 
              const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    
    // Help is handled by the parser
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::setup(const clause_options &options, 
               const clause_subjects &subjects) 
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    

    bool json_mode = options.count("json") > 0;

    auto emit_json = [&](int success, const std::string &message) {
        cout 
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
                else cerr << "Error: db-size must be a positive integer.\n";
                errcode = 1;
                RETURN_COMMANDSTREAMS
            }
        } catch (...) {
            if (json_mode) emit_json(0, "Error: Invalid db-size value. Must be a positive integer.");
            else cerr << "Error: Invalid db-size value. Must be a positive integer.\n";
            errcode = 1;
            RETURN_COMMANDSTREAMS
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
            else cerr << "Failed to remove " << path << ": " << ec.message() << "\n";
        } else if (!removed) {
            if (json_mode) emit_json(0, "Nothing removed (file did not exist): " + path);
            else cerr << "Nothing removed (file did not exist): " << path << "\n";
        } else {
            if (json_mode) emit_json(1, "Removed config: " + path);
            else cout << "Removed config: " << path << "\n";
        }
        errcode = 0;

        RETURN_COMMANDSTREAMS
    }

    // Normal setup
    std::string config_path = bk_mgmt::beessetup(uuid, db_size);
    if (!config_path.empty()) {
        if (json_mode) emit_json(1, "Configuration created/updated: " + config_path);
        else cout << "Configuration created/updated: " << config_path << "\n";
        errcode = 0;
        RETURN_COMMANDSTREAMS
    } else {
        if (json_mode) emit_json(0, "Error: Failed to create/update configuration");
        else cerr << "Error: Failed to create/update configuration\n";
        RETURN_COMMANDSTREAMS
    }
}

command_streams
clauses::locate(const clause_options &options,
                const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    
    bool json_output = options.find("json") != options.end();

    if (json_output) {
        // --- JSON output ---
        cout << "{";

        bool first_uuid = true;
        for (const auto &uuid : subjects) {
            clause_subjects mountpoints = bk_mgmt::get_mount_paths(uuid);

            if (!first_uuid) {
                cout << ", ";
            }
            first_uuid = false;

            cout << "\"" << bk_util::json_escape(uuid) << "\": [";

            bool first_mp = true;
            for (const auto &mp : mountpoints) {
                if (!first_mp) {
                    cout << ", ";
                }
                first_mp = false;
                cout << "\"" << bk_util::json_escape(mp) << "\"";
            }

            cout << "]";
        }

        cout << "}" << std::endl;
    } else {
        // --- Pretty-printed human output ---
        for (const auto &uuid : subjects) {
            clause_subjects mountpoints = bk_mgmt::get_mount_paths(uuid);

            if (!mountpoints.empty()) {
                cout << "Points that " << uuid << " is mounted on:" << std::endl;
                for (const auto &mp : mountpoints) {
                    cout << "\t" << mp << std::endl;
                }
            } else {
                cerr << uuid << ": not mounted or not found" << std::endl;
            }
        }
    }

    errcode = 0;

    RETURN_COMMANDSTREAMS
}

command_streams
clauses::list(const clause_options &options,
              const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    

    fs_map filesystems = bk_mgmt::btrfsls();

    bool want_json = (options.find("json") != options.end());

    if (want_json) {
        // Emit compact JSON array for machine consumption
        std::ostringstream out;
        out << '[';

        bool is_first = true;
        for (const auto &[uuid, info] : filesystems) {
            if (!is_first) {
                out << ",";
            }

            is_first = false;

            std::string label   = info.label;
            std::string status  = info.status;
            std::string devname = info.devname;
            std::string config_path = info.config;
            bool compressing = info.compressing;
            bool autostart = info.autostart;

            out << '{';
            out << "\"uuid\":\""    << bk_util::json_escape(uuid)    << "\",";
            out << "\"label\":\""   << bk_util::json_escape(label)   << "\",";
            out << "\"status\":\""  << bk_util::json_escape(status)  << "\",";
            out << "\"devname\":\"" << bk_util::json_escape(devname) << "\",";
            out << "\"config\":\""  << bk_util::json_escape(config_path) << "\",";
            out << "\"compressing\":" << (compressing ? "true" : "false") << ",";
            out << "\"autostart\":" << (autostart ? "true" : "false") << "";
            out << '}';
        }

        out << ']';
        cout << out.str() << std::endl;
        errcode = 0;
        RETURN_COMMANDSTREAMS
    }

    // -------------------------
    // Pretty-table (human readable)
    // -------------------------
    if (filesystems.empty()) {
        cout << "No btrfs filesystems found.\n";
        errcode = 0;
        RETURN_COMMANDSTREAMS
    }

    // Precompute all status strings
    clause_subjects status_lines;
    size_t status_width = 7; // "STATUS" header length

    for (const auto &[uuid, info] : filesystems) {
        std::string status = (info.status.empty() ? "unknown" : info.status);
        size_t status_length = status.length();
        status_lines.push_back(std::move(status));
        status_width = std::max(status_width, status_length);
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

    for (const auto &[uuid, info] : filesystems) {
        uuid_width  = std::max(uuid_width,  uuid.length());
        label_width = std::max(label_width, info.label.length());
    }

    // Apply constraints
    uuid_width  = std::min(std::max(uuid_width,  MIN_UUID_LEN),  MAX_UUID_LEN);
    label_width = std::min(std::max(label_width, MIN_LABEL_LEN), MAX_LABEL_LEN);
    status_width = std::min(std::max(status_width, MIN_STATUS_LEN), MAX_STATUS_LEN);

    // Table header
    cout << std::left
              << std::setw(uuid_width)  << "UUID"   << " "
              << std::setw(label_width) << "LABEL"  << " "
              << std::setw(status_width) << "STATUS"
              << "\n";

    // Separator line
    cout << std::string(uuid_width, '-')  << " "
              << std::string(label_width, '-') << " "
              << std::string(status_width, '-') << "\n";

    // Print rows
    for (const auto &[src_uuid, info] : filesystems) {
        std::string uuid = src_uuid; // mutable copy
        std::string label  = info.label;
        std::string status = info.status;

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

        if (status.length() > status_width) {
            status = status.substr(0, status_width - 3) + "...";
        }

        cout << "\n";

        cout << uuid << " "
                  << label << " "
                  << status;
    }

    errcode = 0;
    RETURN_COMMANDSTREAMS
}

command_streams
clauses::stat(const clause_options &options,
              const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    
    if (subjects.empty()) {
        cerr << "Error: UUID not specified" << std::endl;
        errcode = 1;
        RETURN_COMMANDSTREAMS
    }

    auto emit_json = [&](bool success, const std::string &key, const std::string &val) {
        cout << "{\n"
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
            else cout << bk_util::auto_size_suffix(free_val) << std::endl;
            errcode = 0;
            RETURN_COMMANDSTREAMS
        } else if (mode == "used") {
            if (json) emit_json(true, "used", std::to_string(used_val));
            else cout << bk_util::auto_size_suffix(used_val) << std::endl;
            errcode = 0;
            RETURN_COMMANDSTREAMS
        } else {
            if (json) {
                cout << "{\n"
                          << "  \"success\": 1,\n"
                          << "  \"free\": " << free_val << ",\n"
                          << "  \"used\": " << used_val << "\n"
                          << "}\n";
            } else {
                cout << "Free space: " << bk_util::auto_size_suffix(free_val) << std::endl;
                cout << "Used space: " << bk_util::auto_size_suffix(used_val) << std::endl;
            }
            errcode = 0;
            RETURN_COMMANDSTREAMS
        }
    }

    // Config check
    std::string config_path = bk_mgmt::btrfstat(uuid);
    if (!config_path.empty()) {
        if (json) emit_json(true, "config_path", config_path);
        else cout << "Configuration exists: " << config_path << std::endl;
        errcode = 0;
        RETURN_COMMANDSTREAMS
    } else {
        if (json) emit_json(false, "config_path", "");
        else cout << "No configuration found for " << uuid << std::endl;
        errcode = 1;
        RETURN_COMMANDSTREAMS;
    }
}

command_streams
clauses::autostartctl(const clause_options &options,
                      const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    
    bool add = options.find("add") != options.end();
    bool remove = options.find("remove") != options.end();

    for (const std::string &uuid_str : subjects) {
        if (add)
            bk_mgmt::autostart::add_uuid(uuid_str);
        else if (remove)
            bk_mgmt::autostart::remove_uuid(uuid_str);
    }

    RETURN_COMMANDSTREAMS
}

command_streams
clauses::compressctl(const clause_options &options,
                     const clause_subjects &subjects)
{
    std::ostringstream cout;
    std::ostringstream cerr;
    int errcode = 0;
    
    namespace tc = bk_mgmt::transparentcompression;

    bool start  = options.find("start")  != options.end() || options.find("s") != options.end();
    bool pause  = options.find("pause")  != options.end() || options.find("p") != options.end();
    bool status = options.find("status") != options.end() || options.find("i") != options.end();
    bool add    = options.find("add")    != options.end() || options.find("a") != options.end();
    bool remove = options.find("remove") != options.end() || options.find("r") != options.end();

    bool want_json = (options.find("json") != options.end()) || (options.find("j") != options.end());

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
        cout << "[";
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
                if (!first_json_item) cout << ",";
                first_json_item = false;

                cout << "\n  {"
                          << "\"uuid\":\"" << uuid_str << "\","
                          << "\"enabled\":" << (enabled ? "true" : "false") << ","
                          << "\"running\":" << (running ? "true" : "false") << ","
                          << "\"algorithm\":\"" << algorithm << "\","
                          << "\"level\":\"" << level_str << "\""
                          << "}";
            } else {
                cout << uuid_str << ": "
                          << (enabled ? "Enabled to automatically compress at boot; "
                                      : "Disabled to automatically compress at boot; ")
                          << (running ? "compressing" : "paused, not running");

                if (!algorithm.empty() && algorithm != "none") {
                    cout << " with algorithm " << algorithm;
                    if (!level_str.empty() && level_str != "0") {
                        cout << " at level " << level_str;
                    }
                }

                cout << std::endl;
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
        if (!first_json_item) cout << "\n";
        cout << "]" << std::endl;
    }

    errcode = 0;
    RETURN_COMMANDSTREAMS
}
