#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/util.hpp"
#include "beekeeper/debug.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace tc = beekeeper::management::transparentcompression;

bool
tc::is_enabled_for(const std::string &uuid)
{
    return configfile::is_present(transparentcompression_config_file, uuid);
}

/**
 * @brief Add or update a UUID entry for transparent compression.
 *
 * This function writes an entry to the transparent compression configuration
 * file with the format:
 * @code
 * <uuid> <algorithm> [level]
 * @endcode
 *
 * Supported algorithms and valid level ranges:
 * - "zlib": 1 to 9
 * - "lzo": levels are ignored (level will always be normalized to 0)
 * - "zstd": -15 to 15
 *
 * Levels outside of the valid range are automatically normalized to the nearest
 * valid value (similar to `btrfs mount` behavior):
 * - zlib: levels <1 become 1, levels >9 become 9
 * - zstd: levels <-15 become -15, levels >15 become 15
 * - lzo: always 0
 *
 * @param uuid The UUID of the filesystem to enable transparent compression on.
 * @param algorithm The compression algorithm ("zlib", "lzo", "zstd").
 * @param level The compression level (ignored for lzo). Will be normalized
 *              if out of valid bounds.
 */
void
tc::add_uuid(const std::string &uuid, const std::string &algorithm, int level)
{
    if (uuid.empty())
        return;

    std::string algo = bk_util::to_lower(bk_util::trim_string(algorithm));
    bool valid = false;

    // Normalize levels per algorithm
    if (algo == "zlib") {
        if (level < 1) level = 1;
        else if (level > 9) level = 9;
        valid = true;
    }
    else if (algo == "lzo") {
        level = 0;  // ignored
        valid = true;
    }
    else if (algo == "zstd") {
        if (level < -15) level = -15;
        else if (level > 15) level = 15;
        valid = true;
    }

    if (!valid) {
        DEBUG_LOG("[tc::add_uuid] Invalid compression algorithm: " + algo);
        return;
    }

    DEBUG_LOG("[tc::add_uuid] Adding UUID " + uuid + " with algo=" + algo + 
              " level=" + std::to_string(level));

    // Stitch line as "uuid algorithm level" if level != 0
    if (level != 0)
        configfile::add(transparentcompression_config_file, uuid, algo, std::to_string(level));
    else
        configfile::add(transparentcompression_config_file, uuid, algo);
}

void
tc::remove_uuid(const std::string &uuid)
{
    configfile::remove_line_matching_substring(transparentcompression_config_file, uuid);
}

bool
tc::start(const std::string &uuid)
{
    if (uuid.empty()) {
        DEBUG_LOG("[transparentcompression] start: empty uuid");
        return false;
    }

    // 1) Resolve all mountpoints for UUID
    std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);
    if (mountpoints.empty()) {
        DEBUG_LOG("[transparentcompression] start: no mountpoints found for uuid " + uuid);
        return false;
    }

    // 2) Fetch config for uuid (first matching line)
    std::string algo;
    int level = 0;
    auto cfg_lines = bk_mgmt::configfile::fetch(
        bk_mgmt::transparentcompression_config_file,
        uuid,
        /*case_insensitive=*/true,
        /*max_coincidence_lines_count=*/1
    );

    if (!cfg_lines.empty()) {
        std::string config_line = cfg_lines[0];
        auto tokens = bk_util::tokenize(config_line, ' ');
        if (tokens.size() >= 2) algo = bk_util::trim_string(tokens[1]);
        if (tokens.size() == 3) {
            try { level = std::stoi(bk_util::trim_string(tokens[2])); }
            catch (...) { level = 0; }
        }
    }

    // 3) Apply defaults
    algo = bk_util::to_lower(bk_util::trim_string(algo));
    if (algo.empty()) algo = "lzo";

    // 4) Build compression token
    std::string compression_token = std::string("compress=") + algo;
    if (level != 0) compression_token += ":" + std::to_string(level);

    // 5) Remount all mountpoints, skipping those where compression is already running
    bool res = bk_mgmt::remount_in_place(
        mountpoints,
        compression_token,
        [algo, level](const std::string &mp) {
            // Get the current algorithm and level
            auto [current_algo, current_level] = get_current_compression_level(mp);

            // If destination algorithm doesn't match with the current one, don't skip
            if (current_algo != algo) return false;

            // If destination level doesn't match the current one, don't skip
            if (current_level == "") current_level = "0";
            if (current_level != std::to_string(level)) return false;

            // Skip if both algorithm and level match
            return true;
        }
    );

    if (!res) {
        std::cerr << "transparentcompression remount failed for " << uuid << std::endl;
        return false;
    }

    std::cerr << "transparentcompression remount succeeded for " << uuid
              << " with " << compression_token << std::endl;
    return true;
}

bool
tc::pause(const std::string &uuid)
{
    if (uuid.empty()) {
        DEBUG_LOG("[transparentcompression] pause: empty uuid");
        return false;
    }

    // Resolve mountpoints
    std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);
    if (mountpoints.empty()) {
        DEBUG_LOG("[transparentcompression] pause: uuid not mounted, nothing to stop: " + uuid);
        return true;
    }

    const std::string opt = "compress=none";

    bool res = bk_mgmt::remount_in_place(mountpoints, opt);

    if (!res) {
        std::cerr << "transparentcompression pause failed for " << uuid << std::endl;
        return false;
    }

    DEBUG_LOG("[transparentcompression] compression paused for " + uuid);
    return true;
}

// Helper: unescape octal sequences as used in /proc/mounts (e.g. "\040" -> ' ')
static
std::string unescape_proc_mount_field(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size()
            && std::isdigit(static_cast<unsigned char>(s[i+1]))
            && std::isdigit(static_cast<unsigned char>(s[i+2]))
            && std::isdigit(static_cast<unsigned char>(s[i+3])))
        {
            // three octal digits
            int v = (s[i+1]-'0')*64 + (s[i+2]-'0')*8 + (s[i+3]-'0');
            out.push_back(static_cast<char>(v));
            i += 3;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

/**
 * @brief Check if transparent compression is active.
 *
 * Accepts either:
 *  - a filesystem UUID (recognized by bk_util::is_uuid), in which case all mountpoints
 *    for that UUID are resolved and checked.
 *  - a mountpoint (or device path), in which case that mountpoint is directly checked.
 *
 * This is now just a wrapper around bk_mgmt::get_current_compression_level():
 * if any mountpoint reports a non-empty and non-"none" algorithm, the filesystem
 * is considered to have compression running.
 *
 * @param uuid_or_mountpoint_or_device UUID, mountpoint, or device path.
 * @return true if transparent compression is active for at least one of the resolved mounts.
 */
bool
tc::is_running(const std::string &uuid_or_mountpoint_or_device)
{
    if (uuid_or_mountpoint_or_device.empty()) {
        DEBUG_LOG("[transparentcompression] is_running: empty input");
        return false;
    }

    // 1. Build list of mountpoints to inspect
    std::vector<std::string> mountpoints;
    if (bk_util::is_uuid(uuid_or_mountpoint_or_device)) {
        mountpoints = bk_mgmt::get_mount_paths(uuid_or_mountpoint_or_device);
    } else {
        mountpoints.push_back(uuid_or_mountpoint_or_device);
    }

    if (mountpoints.empty()) {
        // Not mounted -> cannot be running
        return false;
    }

    // 2. For every mountpoint, check compression algorithm
    for (const auto &mp : mountpoints) {
        auto [algo, lvl] = bk_mgmt::transparentcompression::get_current_compression_level(mp);
        if (!algo.empty() && algo != "none") {
            return true;
        }
    }

    return false;
}

/**
 * @brief Return true if at least one mountpoint (of the given UUID) does NOT have
 * transparent compression running.
 *
 * If the given string is a UUID, resolves all mountpoints and checks them.
 * If any resolved mountpoint is missing compression, returns true.
 */
bool
tc::is_not_running_for_at_least_one_mountpoint_of(const std::string &uuid)
{
    if (uuid.empty()) return true; // nothing mounted = considered "not running"

    // Resolve all mountpoints for UUID
    std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);
    if (mountpoints.empty()) return true; // not mounted => considered "not running" for at least one

    for (const auto &mp : mountpoints) {
        if (!tc::is_running(mp)) {
            // found at least one mountpoint where compression is not running
            return true;
        }
    }

    // All mountpoints have compression running
    return false;
}

/**
 * @brief Inspect /proc/mounts to determine the current compression algorithm and level
 *        used by a mounted btrfs filesystem.
 *
 * Input can be either:
 *  - a UUID (recognized via bk_util::is_uuid), in which case all mountpoints for that
 *    UUID are resolved and the first one with a "compress=" option is used as reference.
 *  - a mountpoint string, in which case /proc/mounts is searched for a line with both
 *    that mountpoint and "compress=".
 *
 * Parsing logic:
 *  - The reference /proc/mounts line is split into tokens.
 *  - The first token is stored as the reference device/mountpoint.
 *  - The token containing "compress=" is located.
 *  - If that token has the form "compress=algorithm:level", algorithm and level are split.
 *  - If it is "compress=algorithm" with no colon, only algorithm is returned.
 *
 * @note Either algorithm or level (or both) may be empty depending on what is found.
 *       Callers should apply their own defaults when empty values are returned.
 *
 * @param mountpoint_or_uuid A UUID or a mountpoint string.
 * @return std::pair<algorithm, level>
 */
std::pair<std::string, std::string>
tc::get_current_compression_level(const std::string &mountpoint_or_uuid)
{
    std::string reference_mountpoint_line;
    std::string reference_mountpoint;
    std::string algorithm;
    std::string level;

    if (mountpoint_or_uuid.empty()) {
        return {algorithm, level};
    }

    // Collect candidate mountpoints
    std::vector<std::string> mountpoints;
    if (bk_util::is_uuid(mountpoint_or_uuid)) {
        mountpoints = bk_mgmt::get_mount_paths(mountpoint_or_uuid);
    } else {
        mountpoints.push_back(mountpoint_or_uuid);
    }

    if (mountpoints.empty()) {
        return {algorithm, level};
    }

    // Try to find a mountpoint line that includes both the mountpoint and "compress="
    for (const auto &possible_mountpoint : mountpoints) {
        std::vector<std::string> matched = bk_util::find_lines_matching_substring_in_file(
            "/proc/mounts",
            std::vector<std::string>{possible_mountpoint, "compress="},
            false,
            1 // only need the first match
        );

        if (!matched.empty()) {
            reference_mountpoint_line = matched[0];
            break;
        }
    }

    if (reference_mountpoint_line.empty()) {
        // No compression-enabled mountpoint found
        return {algorithm, level};
    }

    // Tokenize the reference line
    std::vector<std::string> tokens = bk_util::tokenize(reference_mountpoint_line);
    if (tokens.empty()) {
        return {algorithm, level};
    }

    reference_mountpoint = tokens[0];

    // Find the compress= option token
    for (const auto &tok : tokens) {
        if (tok.find("compress=") != std::string::npos) {
            size_t eq_pos = tok.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string comp_value = tok.substr(eq_pos + 1);

            size_t colon_pos = comp_value.find(':');
            if (colon_pos != std::string::npos) {
                algorithm = comp_value.substr(0, colon_pos);
                level = comp_value.substr(colon_pos + 1);
            } else {
                algorithm = comp_value;
            }
            break;
        }
    }

    return {algorithm, level};
}
