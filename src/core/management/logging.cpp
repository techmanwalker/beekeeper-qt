#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/debug.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Helper: Get log directory path
std::string
bk_mgmt::get_log_dir ()
{
    return "/var/log/beesd/";
}

// Helper: Get log file path
std::string
bk_mgmt::get_log_path (const std::string& uuid)
{
    return get_log_dir() + uuid + ".log";
}

// Helper: Create log directory if needed
void
bk_mgmt::ensure_log_dir ()
{
    std::string log_dir = bk_mgmt::get_log_dir();
    if (!bk_util::file_exists(log_dir)) {
        fs::create_directories(log_dir);
        // Owner: rwx, Group: r-x, Others: r-x
        fs::permissions(
            log_dir,
            fs::perms::owner_all |
            fs::perms::group_read | fs::perms::group_exec |
            fs::perms::others_read | fs::perms::others_exec,
            fs::perm_options::replace
        );
    }
}

void
bk_mgmt::clear_log_file_for_uuid(const std::string &uuid)
{
    std::string log_path = bk_mgmt::get_log_path(uuid);
    if (bk_util::file_exists(log_path)) {
        try {
            if (fs::remove(log_path)) {
                DEBUG_LOG("Successfully removed old log file: ", log_path);
            } else {
                DEBUG_LOG("Failed to remove log file: ", log_path);
                std::cerr << "Warning: Failed to remove existing log file: " << log_path << std::endl;
            }
        } catch (const fs::filesystem_error &e) {
            DEBUG_LOG("Exception removing log: ", e.what());
            std::cerr << "Warning: Failed to remove log file: " << e.what() << std::endl;
        }
    }
}

void
bk_mgmt::create_started_with_n_gb_file (const std::string &uuid)
{
    unsigned long long free_bytes = bk_mgmt::get_space::free(uuid);

    DEBUG_LOG(uuid, " FREE BYTES: ", std::to_string(free_bytes));

    fs::path start_file = fs::path("/tmp") / ".beekeeper" / uuid / "startingfreespace";

    // Create parent directories if needed
    fs::create_directories(start_file.parent_path());

    DEBUG_LOG("Attempted to create ", start_file.parent_path(), ". Does it exist? ", (fs::exists(start_file.parent_path()) ? "yes" : "no"));
    // Only create starting free space file if it doesn't exist
    if (!fs::exists(start_file)) {
        if (free_bytes > 0) {
            std::ofstream ofs(start_file);
            if (ofs.is_open()) {
                ofs << free_bytes;
            }
        } else {
            DEBUG_LOG("handle_start: free_bytes is zero for UUID " + uuid);
        }
    }
}