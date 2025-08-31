#include "../../include/beekeeper/btrfsetup.hpp"
#include "../../include/beekeeper/internalaliases.hpp" // required for bk_util
#include "../../include/beekeeper/util.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>

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
std::vector<std::map<std::string, std::string>>
beekeeper::management::btrfsls ()
{
    std::vector<std::map<std::string, std::string>> filesystems;
    std::string output = bk_util::exec_command("btrfs filesystem show -d").stdout_str;
    
    if (output.empty()) {
        return filesystems;
    }
    
    // Split output into lines
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Look for UUID in the line (must be present)
        if (line.find("uuid:") == std::string::npos) {
            continue;
        }
        
        std::map<std::string, std::string> fs_info;
        std::string label_str = "";
        std::string uuid_str = "";
        
        // Extract UUID first (always required)
        size_t uuid_pos = line.find("uuid:") + 5;
        if (uuid_pos != std::string::npos) {
            // Skip any whitespace after uuid:
            while (uuid_pos < line.size() && std::isspace(line[uuid_pos])) {
                uuid_pos++;
            }
            
            // Find end of UUID (either space or end of line)
            size_t uuid_end = uuid_pos;
            while (uuid_end < line.size() && !std::isspace(line[uuid_end])) {
                uuid_end++;
            }
            
            uuid_str = line.substr(uuid_pos, uuid_end - uuid_pos);
            fs_info["uuid"] = uuid_str;
        }
        
        // Extract label if present
        if (line.find("Label:") != std::string::npos) {
            size_t label_start = line.find('\'');
            size_t label_end = line.find('\'', label_start + 1);
            
            if (label_start != std::string::npos && label_end != std::string::npos) {
                label_str = line.substr(label_start + 1, label_end - label_start - 1);
            } else {
                // Fallback for unquoted labels
                size_t label_pos = line.find("Label:") + 6;
                if (label_pos != std::string::npos) {
                    // Extract until "uuid:" or end of line
                    size_t label_end = line.find("uuid:");
                    if (label_end == std::string::npos) label_end = line.size();
                    
                    label_str = line.substr(label_pos, label_end - label_pos);
                    
                    // Trim trailing whitespace
                    size_t last_char = label_str.find_last_not_of(" \t");
                    if (last_char != std::string::npos) {
                        label_str = label_str.substr(0, last_char + 1);
                    }
                }
            }
        }
        
        // Use UUID as label if no label was found
        if (label_str.empty()) {
            label_str = uuid_str;
        }
        
        fs_info["label"] = label_str;
        
        // Only add if we have a UUID
        if (!uuid_str.empty()) {
            filesystems.push_back(fs_info);
        }
    }
    
    return filesystems;
}

// Check if config exists for btrfs UUID
std::string
beekeeper::management::btrfstat (std::string uuid)
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
            if (!found_uuid.empty() && bk_util::string_equal_ignore_case(found_uuid, uuid)) {
                return entry.path().string();
            }
        }
    }
    
    return "";
}

// Create/update config file
std::string
bk_mgmt::beessetup (std::string uuid, size_t db_size)
{
    // Check if config exists
    std::string config_path = beekeeper::management::btrfstat(uuid);
    bool config_exists = !config_path.empty();
    std::map<std::string, std::string> new_config;
    
    // Warn about comments being removed
    // Handle existing config
    if (config_exists) {
        std::cout << "Warning: removing configuration file comments" << std::endl;
        new_config = parse_config(config_path);
    }
    
    // Always set UUID
    new_config["UUID"] = uuid;
    
    // Handle DB_SIZE
    if (db_size > 0)
    {
        // Explicit size specified - always set
        new_config["DB_SIZE"] = std::to_string(db_size);
    }
    else if (config_exists)
    {
        // Preserve existing DB_SIZE if available
        if (new_config.find("DB_SIZE") == new_config.end()) {
            // Missing DB_SIZE - add default with warning
            std::cout << "Warning: adding missing DB_SIZE to existing config" << std::endl;
            new_config["DB_SIZE"] = "1073741824";
        }
        // Otherwise keep existing DB_SIZE
    }
    else
    {
        // New config with default size
        new_config["DB_SIZE"] = "1073741824";
    }

    // Ensure /etc/bees directory exists
    const fs::path conf_dir = "/etc/bees";
    std::error_code ec;
    
    if (!fs::exists(conf_dir, ec))
    {
        if (!fs::create_directories(conf_dir, ec) || ec)
        {
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
    if (!out.is_open())
    {
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