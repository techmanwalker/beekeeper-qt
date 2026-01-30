#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/internalaliases.hpp"
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "beekeeper/util.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>

#ifdef HAVE_LIBBLKID
  #include <cstring>
  extern "C" {
    #include <blkid/blkid.h>
  }
#endif

namespace fs = std::filesystem;

// Extract UUID value from config line
static std::string
extract_uuid (const std::string& line)
{
    const std::string prefix = "UUID=";
    
    // Find UUID= prefix
    size_t pos = line.find(prefix);
    if (pos == std::string::npos) return "";
    
    // Extract UUID value
    std::string uuid_value = line.substr(pos + prefix.length());
    
    // Remove trailing whitespace/comments
    size_t endpos = uuid_value.find_first_of(" \t#");
    if (endpos != std::string::npos) {
        uuid_value = uuid_value.substr(0, endpos);
    }
    
    // Remove any quotes
    if (!uuid_value.empty() && uuid_value.front() == '"') {
        uuid_value.erase(0, 1);
    }
    if (!uuid_value.empty() && uuid_value.back() == '"') {
        uuid_value.pop_back();
    }
    
    return uuid_value;
}

// Parse config file into key-value map
static std::map<std::string, std::string>
parse_config (const fs::path& config_path)
{
    std::map<std::string, std::string> config;
    std::ifstream file(config_path);
    std::string line;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        size_t sep_pos = line.find('=');
        if (sep_pos == std::string::npos) continue;
        
        // Split in key-value pairs
        std::string key = line.substr(0, sep_pos);
        std::string value = line.substr(sep_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        config[key] = value;
    }
    return config;
}

// List btrfs filesystems
/**
 * @brief List available Btrfs filesystems.
 *
 * This function asks blkid for UUID, LABEL and TYPE for all block devices,
 * then filters lines that contain TYPE="btrfs" (case-insensitive).
 *
 * For every matching line it extracts:
 *  - uuid -> from token "UUID=\"...\"" (or "" if missing)
 *  - label -> from token "LABEL=\"...\"" (or "" if missing)
 *  - status -> result of bk_mgmt::beesstatus(uuid) (or "" if uuid empty)
 *
 * Tokenization of each blkid line is performed with bk_util::tokenize(..., ' ')
 * so quoted fields are preserved. Quotes are removed using bk_util::trip_quotes().
 *
 * @return Vector of maps; each map contains keys "uuid", "label" and "status".
 */
fs_map
bk_mgmt::btrfsls()
{
    fs_map available_filesystems;

#ifdef HAVE_LIBBLKID
    DEBUG_LOG("If you can see this, libblkid was compiled into beekeeper-qt.");

    blkid_cache cache = nullptr;
    if (blkid_get_cache(&cache, nullptr) < 0) {
        DEBUG_LOG("blkid_get_cache() failed.");
        return available_filesystems;
    }

    blkid_dev dev;
    blkid_dev_iterate iter = blkid_dev_iterate_begin(cache);

    while (blkid_dev_next(iter, &dev) == 0) {
        const char *devname = blkid_dev_devname(dev);

        if (!blkid_dev_has_tag(dev, "TYPE", "btrfs"))
            continue;

        char *uuid  = blkid_get_tag_value(cache, "UUID", devname);
        char *label = blkid_get_tag_value(cache, "LABEL", devname);

        std::pair<std::string, fs_info> entry;

        if (uuid)    entry.first    = uuid;
        if (devname) entry.second.devname = devname;
        if (label)   entry.second.label   = label;

        // also fetch .status using beesstatus(uuid)
        if (uuid) {
            entry.second.status = bk_mgmt::beesstatus(uuid);
            entry.second.config = bk_mgmt::btrfstat(uuid);
            entry.second.compressing = bk_mgmt::transparentcompression::is_running(uuid);
            entry.second.autostart = bk_mgmt::autostart::is_enabled_for(uuid);
        } else {
            entry.second.status = "unknown";
            entry.second.config = "unknown";
            entry.second.compressing = "unknown";
            entry.second.autostart = "unknown";
        }

        DEBUG_LOG("BLKID found fs: \n",
        "    uuid: ", entry.first, "\n",
        "    devname: ", entry.second.devname, "\n",
        "    label: ", entry.second.label, "\n",
        "    status: ", entry.second.status, "\n",
        "    config: ", entry.second.config, "\n",
        "    compressing: ", entry.second.compressing, "\n",
        "    autostart: ", entry.second.autostart, "\n",
        "");

        // if uuid is empty, weird notation
        if (uuid && *uuid)
            available_filesystems.insert(std::move(entry));

        if (uuid)  free(uuid);
        if (label) free(label);
    }

    blkid_dev_iterate_end(iter);
    blkid_put_cache(cache);

#else

    // Fallback: shell out to blkid and parse lines
    {
        command_streams res = bk_util::exec_command("blkid", "-s", "UUID", "-s", "LABEL", "-s", "TYPE");
        // DEBUG_LOG("blkid response: ", res.stdout_str);
        std::vector<std::string> filesystem_lines = bk_util::split_command_streams_by_lines(res).first;
        std::vector<std::string> btrfs_lines =
            bk_util::find_lines_matching_substring_in_vector(filesystem_lines, "TYPE=\"btrfs\"", true);
        DEBUG_LOG("blkid lines: ", bk_util::serialize_vector(btrfs_lines));

        for (const auto &line : btrfs_lines) {
            std::pair<std::string, fs_info> entry;
            entry.first   = "";
            entry.second.label = "";
            entry.second.status = "";
            entry.second.devname = "";

            std::vector<std::string> fs_tokens = bk_util::tokenize(line, ' ');
            DEBUG_LOG("tokenized line: ", bk_util::serialize_vector(fs_tokens));
            std::string peeled_uuid;
            std::string peeled_label;

            for (const auto &tok : fs_tokens) {
                if (tok.rfind("UUID=", 0) == 0) {
                    peeled_uuid = bk_util::trip_quotes(tok.substr(5));
                } else if (tok.rfind("LABEL=", 0) == 0) {
                    peeled_label = bk_util::trip_quotes(tok.substr(6));
                }
            }

            entry.first  = peeled_uuid;
            entry.second.label = peeled_label;

            if (!peeled_uuid.empty()) {
                entry.second.status  = bk_mgmt::beesstatus(peeled_uuid);
                entry.second.devname = bk_mgmt::get_real_device(peeled_uuid);
                entry.second.config = bk_mgmt::btrfstat(peeled_uuid);
                entry.second.compressing = bk_mgmt::transparentcompression::is_running(peeled_uuid);
                entry.second.autostart = bk_mgmt::autostart::is_enabled_for(peeled_uuid);
            } else {
                entry.second.status  = "unknown";
                entry.second.devname = "unknown";
                entry.second.config = "unknown";
                entry.second.compressing = "unknown";
                entry.second.autostart = "unknown";
            }

            DEBUG_LOG("BLKID found fs: ",
            "    uuid: ", entry.first, "\n",
            "    devname: ", entry.second.devname, "\n",
            "    label: ", entry.second.label, "\n",
            "    status: ", entry.second.status, "\n",
            "    config: ", entry.second.config, "\n",
            "    compressing: ", entry.second.compressing, "\n",
            "    autostart: ", entry.second.autostart, "\n",
            "");

            if (!peeled_uuid.empty())
                available_filesystems.insert(std::move(entry));
        }
    }
#endif
    return available_filesystems;
}

// Check if config exists for btrfs UUID
std::string
bk_mgmt::btrfstat (std::string uuid)
{
    const fs::path conf_dir = "/etc/bees";
    
    if (!fs::exists(conf_dir)) {
        return "";
    }

    for (const auto& entry : fs::recursive_directory_iterator(conf_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".conf") continue;

        std::ifstream file(entry.path());
        std::string line;
        
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            // Extract and check UUID
            std::string found_uuid = extract_uuid(line);
            if (!found_uuid.empty() && bk_util::compare_strings_case_insensitive (found_uuid, uuid)) {
                return entry.path().string();
            }
        }
    }
    
    return "";
}

// Create/update config file for a given UUID and database size
std::string
bk_mgmt::beessetup(std::string uuid, size_t db_size)
{
    // Check if config already exists
    std::string config_path = bk_mgmt::btrfstat(uuid);
    bool config_exists = !config_path.empty();
    std::map<std::string, std::string> new_config;

    // Warn about comments being removed
    if (config_exists) {
        std::cout << "Warning: removing configuration file comments" << std::endl;
        new_config = parse_config(config_path);
    }

    // Always set UUID
    new_config["UUID"] = uuid;

    // Constants for DB_SIZE
    const size_t MIN_DB_SIZE = 16 * 1024 * 1024;       // 16 MiB
    const size_t DEFAULT_DB_SIZE = 1024 * 1024 * 1024; // 1 GiB

    // Handle DB_SIZE logic
    if (db_size > 0) {
        // Explicit size specified - apply minimum limit
        if (db_size < MIN_DB_SIZE) {
            new_config["DB_SIZE"] = std::to_string(DEFAULT_DB_SIZE);
        } else {
            new_config["DB_SIZE"] = std::to_string(db_size);
        }
    } else {
        // db_size == 0: preserve existing DB_SIZE if present
        if (!(config_exists && !new_config["DB_SIZE"].empty())) {
            new_config["DB_SIZE"] = std::to_string(DEFAULT_DB_SIZE);
        }
        // else: keep existing DB_SIZE
    }

    // Safety check to prevent empty DB_SIZE
    if (new_config["DB_SIZE"].empty()) {
        new_config["DB_SIZE"] = std::to_string(DEFAULT_DB_SIZE);
    }

    // Ensure /etc/bees directory exists
    const fs::path conf_dir = "/etc/bees";
    std::error_code ec;

    if (!fs::exists(conf_dir, ec)) {
        if (!fs::create_directories(conf_dir, ec) || ec) {
            std::cerr << "Error: Failed to create directory " << conf_dir << ": "
                      << ec.message() << std::endl;
            return "";
        }
        fs::permissions(conf_dir, fs::perms::owner_all, ec);
    }

    // Determine output path
    fs::path output_path;
    if (config_exists) {
        output_path = fs::path(config_path);
    } else {
        output_path = conf_dir / (uuid + ".conf");
    }

    // Write config file
    std::ofstream out(output_path);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open " << output_path << " for writing" << std::endl;
        return "";
    }

    // Write UUID first
    out << "UUID=" << new_config["UUID"] << "\n";

    // Write DB_SIZE second
    out << "DB_SIZE=" << new_config["DB_SIZE"] << "\n";

    // Write other keys in alphabetical order
    std::vector<std::string> other_keys;
    for (const auto& pair : new_config) {
        if (pair.first != "UUID" && pair.first != "DB_SIZE") {
            other_keys.push_back(pair.first);
        }
    }
    std::sort(other_keys.begin(), other_keys.end());

    for (const auto& key : other_keys) {
        out << key << "=" << new_config[key] << "\n";
    }

    return output_path.string();
}
