#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"

#include <filesystem>
#include <iostream>
#include <limits.h>
#include <unordered_set>
#include <vector>
#include <sys/stat.h>

// check if a given mountpoint or uuid is a btrfs filesystem
bool
bk_mgmt::is_btrfs(const std::string &mountpoint_or_uuid_or_device)
{
    if (mountpoint_or_uuid_or_device.empty()) return false;

    // If the input is a UUID -> use blkid on the real device
    if (bk_util::is_uuid(mountpoint_or_uuid_or_device)) {
        std::string real_dev = get_real_device(mountpoint_or_uuid_or_device);
        if (real_dev.empty()) return false;

        auto res = bk_util::exec_command("blkid", real_dev);
        // blkid output typically contains tokens like: /dev/xxx: UUID="..." TYPE="btrfs" ...
        if (res.stdout_str.empty()) return false;

        auto tokens = bk_util::tokenize(res.stdout_str);
        for (const auto &t : tokens) {
            // token contains TYPE=... (maybe TYPE="btrfs")
            if (t.rfind("TYPE=", 0) == 0 || t.find("TYPE=") != std::string::npos) {
                // extract after TYPE=, strip quotes
                auto pos = t.find("TYPE=");
                std::string val = t.substr(pos + 5);
                // remove surrounding quotes if present
                if (!val.empty() && val.front() == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }
                auto v = bk_util::to_lower(bk_util::trim_string(val));
                return (v.find("btrfs") != std::string::npos);
            }
        }
        return false;
    }

    // Otherwise assume the input is a mountpoint -> check /proc/mounts for its fstype
    std::error_code ec;
    std::string normalized;
    try {
        normalized = std::filesystem::weakly_canonical(mountpoint_or_uuid_or_device).string();
    } catch (...) {
        normalized = mountpoint_or_uuid_or_device;
    }

    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        std::cerr << "[is_btrfs] failed to open /proc/mounts\n";
        return false;
    }

    std::string line;
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mnt, fstype;
        if (!(iss >> device >> mnt >> fstype)) continue;

        std::string normalized_mnt;
        try {
            normalized_mnt = std::filesystem::weakly_canonical(mnt).string();
        } catch (...) {
            normalized_mnt = mnt;
        }

        if (normalized_mnt == normalized && fstype == "btrfs") {
            return true;
        }
    }

    return false;
}

std::string
bk_mgmt::get_mount_uuid(const std::string &mountpoint)
{
    if (mountpoint.empty())
        return {};

    // Ensure normalized mountpoint
    std::string normalized;
    try {
        normalized = std::filesystem::weakly_canonical(mountpoint).string();
    } catch (...) {
        return {};
    }

    // Get device of the mountpoint
    struct stat st {};
    if (stat(normalized.c_str(), &st) != 0) {
        perror("[get_mount_uuid] stat failed");
        return {};
    }
    dev_t dev_id = st.st_dev;

    // Iterate over /proc/mounts to find matching mountpoint/device
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        std::cerr << "[get_mount_uuid] failed to open /proc/mounts" << std::endl;
        return {};
    }

    std::string line, device, mnt, fstype;
    bool found = false;
    std::string backing_device;
    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        if (!(iss >> device >> mnt >> fstype)) continue;

        // Normalize the mnt path from /proc/mounts
        std::string normalized_mnt;
        try {
            normalized_mnt = std::filesystem::weakly_canonical(mnt).string();
        } catch (...) {
            continue;
        }

        if (normalized_mnt == normalized) {
            backing_device = device; // e.g. /dev/sda2 or /dev/mapper/...
            found = true;
            break;
        }
    }

    if (!found || backing_device.empty())
        return {};

    // Now resolve the UUID via /dev/disk/by-uuid symlinks
    const std::string uuid_dir = "/dev/disk/by-uuid";
    for (const auto &entry : std::filesystem::directory_iterator(uuid_dir)) {
        try {
            std::string uuid = entry.path().filename().string();
            std::string target = std::filesystem::canonical(entry.path()).string();

            if (target == backing_device) {
                return uuid; // return UUID string
            }
        } catch (...) {
            continue;
        }
    }

    return {};
}

/**
 * @brief Remount one or more mountpoints (or UUIDs) in-place with new mount options.
 *
 * This function accepts a vector of strings where each element can be either:
 *  - a mountpoint path (absolute or relative), or
 *  - a filesystem UUID (recognised by bk_util::is_uuid).
 *
 * If an element is a UUID, *all* mountpaths associated with that UUID are resolved
 * (via bk_mgmt::get_mount_paths) and remounted individually. This ensures that when
 * a btrfs filesystem is mounted multiple times (multiple subvolumes), every mount
 * gets the remount operation applied.
 *
 * The function always enforces the "remount" operation in the "-o" argument to mount.
 *
 * Additionally, a user-provided predicate can be passed in (`skip_predicate`).
 * If this predicate returns true for a given mountpoint, that mountpoint is skipped
 * and not remounted. This allows fine-grained control over conditions where remount
 * should not be attempted (e.g. readonly mounts, non-btrfs, etc.).
 *
 * @param mounts_or_uuids Vector of mountpoints or UUIDs to remount.
 * @param remount_options Additional mount options (appended after the mandatory "remount").
 *                        Example: "compress=lzo" -> final -o => "remount,compress=lzo"
 * @param skip_predicate  Optional predicate function. If provided and it returns true
 *                        for a mountpoint, that mountpoint is skipped (not remounted).
 * @return true if all attempted remount operations succeeded (or if nothing needed to be done
 *              and no errors happened), false if any attempted remount failed or inputs were invalid.
 */
bool
bk_mgmt::remount_in_place(
    const std::vector<std::string> &mounts_or_uuids,
    const std::string &remount_options,
    const std::function<bool(const std::string&)> &skip_predicate
)
{
    using namespace std;

    if (mounts_or_uuids.empty()) {
        DEBUG_LOG("[bk_mgmt::remount_in_place] no mounts/uuids provided");
        return false;
    }

    // Resolve inputs into concrete mountpoints (may add multiple mountpoints per UUID).
    std::vector<std::string> actual_mountpoints;
    actual_mountpoints.reserve(mounts_or_uuids.size());

    std::unordered_set<std::string> seen; // deduplicate

    for (const auto &entry : mounts_or_uuids) {
        if (entry.empty()) continue;

        if (bk_util::is_uuid(entry)) {
            // Resolve all mount paths for this UUID (may be multiple)
            std::vector<std::string> paths = bk_mgmt::get_mount_paths(entry);

            if (paths.empty()) {
                DEBUG_LOG("[bk_mgmt::remount_in_place] UUID has no mounts, skipping:", entry);
                continue;
            }

            for (const auto &p : paths) {
                if (p.empty()) continue;
                if (seen.emplace(p).second) {
                    actual_mountpoints.push_back(p);
                }
            }
        } else {
            // Treat entry as a mountpoint string
            if (seen.emplace(entry).second) {
                actual_mountpoints.push_back(entry);
            }
        }
    }

    if (actual_mountpoints.empty()) {
        DEBUG_LOG("[bk_mgmt::remount_in_place] no actual mountpoints resolved (nothing to do)");
        return false;
    }

    // Build -o option: must include "remount"
    const std::string opt = std::string("remount") +
        (remount_options.empty() ? std::string() : (std::string(",") + remount_options));

    if (opt.find("remount") != 0) {
        // defensive: ensure we always send remount as prefix
        DEBUG_LOG("[bk_mgmt::remount_in_place] constructed option does not start with 'remount': ", opt);
        return false;
    }

    DEBUG_LOG("[bk_mgmt::remount_in_place] will remount with -o ", opt);

    // Iterate and remount each mountpoint. Track results.
    bool any_attempted = false;
    bool all_succeeded = true;

    for (const auto &mp : actual_mountpoints) {
        // Minimal validation
        if (mp.empty()) {
            DEBUG_LOG("[bk_mgmt::remount_in_place] skipping empty mountpoint entry");
            continue;
        }
        if (mp.find('\0') != std::string::npos ||
            mp.find('\n') != std::string::npos ||
            mp.find('\r') != std::string::npos) {
            DEBUG_LOG("[bk_mgmt::remount_in_place] invalid mountpoint string (contains control char), skipping:", mp);
            all_succeeded = false;
            continue;
        }

        // Apply skip predicate if provided
        if (skip_predicate && skip_predicate(mp)) {
            DEBUG_LOG("[bk_mgmt::remount_in_place] skip predicate returned true, skipping:", mp);
            continue;
        }

        any_attempted = true;
        DEBUG_LOG("[bk_mgmt::remount_in_place] remounting: ", mp, " opts=", opt);

        auto res = bk_util::exec_command("mount", "-o", opt, mp);

        if (!res.stderr_str.empty()) {
            std::cerr << "remount_in_place failed for " << mp
                      << " (opts: " << opt << "): " << res.stderr_str << std::endl;
            DEBUG_LOG("[bk_mgmt::remount_in_place] remount failed for ", mp, " stderr=", res.stderr_str);
            all_succeeded = false;
        } else {
            DEBUG_LOG("[bk_mgmt::remount_in_place] remount succeeded for ", mp);
        }
    }

    if (!any_attempted) {
        DEBUG_LOG("[bk_mgmt::remount_in_place] nothing was attempted (no valid mountpoints)");
        return false;
    }

    return all_succeeded;
}


std::string
bk_mgmt::get_real_device(const std::string &uuid_or_device)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (uuid_or_device.empty()) return {};

    // If the argument is already a path that looks like /dev/..., return its canonical form
    if (!bk_util::is_uuid(uuid_or_device)) {
        fs::path p(uuid_or_device);
        fs::path canon = fs::canonical(p, ec);
        if (ec) {
            // canonicalization failed — return empty to indicate failure
            return {};
        }
        return canon.string();
    }

    // Otherwise it's a UUID -> resolve /dev/disk/by-uuid/<UUID>
    fs::path uuid_path = fs::path("/dev/disk/by-uuid") / uuid_or_device;
    if (!fs::exists(uuid_path)) {
        return {}; // UUID not present
    }

    fs::path real_device = fs::canonical(uuid_path, ec);
    if (ec) return {};

    // If device is a device-mapper entry (starts with "dm-"), try to map to /dev/mapper/<name>
    // (this mirrors your original logic exactly)
    if (real_device.filename().string().rfind("dm-", 0) == 0) { // starts with dm-
        fs::path sys_dm_name = fs::path("/sys/block") / real_device.filename() / "dm" / "name";
        std::ifstream name_file(sys_dm_name);
        if (name_file.is_open()) {
            std::string dm_name;
            std::getline(name_file, dm_name);
            if (!dm_name.empty()) {
                real_device = fs::path("/dev/mapper") / dm_name;
            }
        }
    }

    return real_device.string();
}

std::vector<std::string>
bk_mgmt::get_mount_paths(const std::string &uuid_or_device)
{
    std::vector<std::string> mountpoints;
    if (uuid_or_device.empty()) return mountpoints;

    // Resolve to a concrete device path if a UUID was provided
    std::string real_device;
    if (bk_util::is_uuid(uuid_or_device)) {
        real_device = get_real_device(uuid_or_device);
        if (real_device.empty()) return mountpoints; // uuid not present / failed resolution
    } else {
        // treat input as device path (or possibly a mountpoint); we'll search /proc/mounts for
        // any line that contains this string (so passing a mountpoint directly still works).
        real_device = uuid_or_device;
    }

    // DEBUG_LOG("Real device for uuid ", uuid_or_device, " is: ", real_device);

    // Find all /proc/mounts lines that contain the real device string (case-insensitive)
    // The helper returns each matching line as a full string.
    auto matched_lines = bk_util::find_lines_matching_substring_in_file(
        "/proc/mounts",
        real_device,
        /*case_insensitive=*/true
    );

    if (matched_lines.empty()) {
        return mountpoints; // nothing found
    }

    // Extract the second token (mountpoint) from each matched line.
    // Use a set to deduplicate while preserving insertion order.
    std::unordered_set<std::string> seen;
    for (const auto &line : matched_lines) {
        if (line.empty()) continue;

        auto tokens = bk_util::tokenize(line, ' ');
        if (tokens.size() < 2) continue;

        std::string mnt = bk_util::trim_string(tokens[1]);

        // Unescape /proc/mounts escapes (e.g. "\040" -> space) if helper exists
        // (we assume bk_util::unescape_proc_mount_field is available as discussed).
        mnt = bk_util::unescape_proc_mount_field(mnt);

        if (mnt.empty()) continue;
        if (seen.emplace(mnt).second) {
            mountpoints.push_back(mnt);
        }
    }

    return mountpoints;
}

/**
 * @brief Return the filesystem label for the given mountpoint or UUID.
 *
 * If @p mountpoint_or_uuid is not a UUID, the function checks whether the
 * path exists and then attempts to obtain the UUID via
 * bk_mgmt::get_mount_uuid(). If a UUID is obtained (or the argument was a
 * UUID to begin with), the function tries to resolve the real device for that
 * UUID and then query the filesystem label using system utilities.
 *
 * Order of attempts:
 *  1. Resolve /dev/disk/by-uuid/<UUID> with realpath() and use that device.
 *  2. If above failed, call bk_mgmt::get_real_device(UUID) to obtain device.
 *  3. If device found, run:
 *       - blkid -s LABEL -o value <device>
 *       - (fallback) lsblk -no LABEL <device>
 *
 * Returns empty string on any failure or if no label is present.
 *
 * @param mountpoint_or_uuid Mountpoint path or a filesystem UUID.
 * @return Filesystem label or empty string when unknown.
 */
std::string
get_filesystem_label(const std::string &mountpoint_or_uuid)
{
    if (mountpoint_or_uuid.empty()) return "";

    // helper: trim whitespace/newlines
    auto trim = [](const std::string &s) -> std::string {
        const char *ws = " \t\r\n";
        size_t b = s.find_first_not_of(ws);
        if (b == std::string::npos) return "";
        size_t e = s.find_last_not_of(ws);
        return s.substr(b, e - b + 1);
    };

    std::string uuid = mountpoint_or_uuid;

    // If not a uuid, treat argument as path: check it exists and get uuid from it.
    if (!bk_util::is_uuid(uuid)) {
        struct stat st;
        if (stat(mountpoint_or_uuid.c_str(), &st) != 0) {
            // path does not exist or inaccessible
            return "";
        }

        uuid = bk_mgmt::get_mount_uuid(mountpoint_or_uuid);
        if (uuid.empty()) return "";
    }

    // Try resolving /dev/disk/by-uuid/<uuid> with realpath()
    std::string syspath = "/dev/disk/by-uuid/" + uuid;
    char resolved[PATH_MAX + 1];
    std::string device_path;
    if (realpath(syspath.c_str(), resolved) != nullptr) {
        device_path = resolved; // absolute device path
    }

    // If realpath failed, try user-provided helper to obtain device path
    if (device_path.empty()) {
        device_path = bk_mgmt::get_real_device(uuid);
    }

    // If we have a device path, try blkid to obtain LABEL
    if (!device_path.empty()) {
        command_streams res = bk_util::exec_command("blkid", "-s", "LABEL", "-o", "value", device_path);
        auto lines = bk_util::split_command_streams_by_lines(res).first;
        if (!lines.empty()) {
            std::string label = trim(lines.front());
            if (!label.empty()) return label;
        }

        // Fallback to lsblk if blkid returned nothing
        command_streams res2 = bk_util::exec_command("lsblk", "-no", "LABEL", device_path);
        auto lines2 = bk_util::split_command_streams_by_lines(res2).first;
        if (!lines2.empty()) {
            std::string label = trim(lines2.front());
            if (!label.empty()) return label;
        }
    }

    // As a last resort, try running blkid on the by-uuid symlink path itself
    // (some systems allow passing the symlink)
    command_streams res3 = bk_util::exec_command("blkid", "-s", "LABEL", "-o", "value", syspath);
    auto lines3 = bk_util::split_command_streams_by_lines(res3).first;
    if (!lines3.empty()) {
        std::string label = trim(lines3.front());
        if (!label.empty()) return label;
    }

    // Not found
    return "";
}

int64_t
bk_mgmt::get_space::free(const std::string &uuid)
{
    std::string mount_path = bk_mgmt::get_mount_paths(uuid)[0];
    if (mount_path.empty()) return -1;

    try {
        auto info = std::filesystem::space(mount_path);
        return static_cast<int64_t>(info.available); // free usable space
    } catch (const std::filesystem::filesystem_error &) {
        return -1;
    }
}

int64_t
bk_mgmt::get_space::used(const std::string &uuid)
{
    std::string mount_path = bk_mgmt::get_mount_paths(uuid)[0];
    if (mount_path.empty()) return -1;

    try {
        auto info = std::filesystem::space(mount_path);
        int64_t used_bytes = static_cast<int64_t>(info.capacity - info.free);
        return used_bytes;
    } catch (const std::filesystem::filesystem_error &) {
        return -1;
    }
}