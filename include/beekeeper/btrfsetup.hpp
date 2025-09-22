#pragma once
#include <map>
#include <string>
#include <vector>

// Trunk beekeeper program
namespace beekeeper {
    // Management subsystem
    namespace management {
        // List btrfs filesystems
        std::vector<std::map<std::string, std::string>>
        btrfsls ();

        // Check if a btrfs filesystem has a corresponding configuration file
        std::string
        btrfstat (std::string uuid);

        // Create a config file in /etc/bees for this filesystem
        // db_size = 0: do not change DB_SIZE if configuration file exists
        std::string
        beessetup (std::string uuid, size_t db_size = 0);

        // Get block device/mapper device for uuid
        std::string
        get_real_device(const std::string &uuid);

        // Return mount point by uuid
        std::vector<std::string> get_mount_paths(const std::string &uuid);

        // Return uuid by mount point
        std::string get_mount_uuid(const std::string &mountpoint);

        namespace get_space {

            int64_t free(const std::string &uuid);

            int64_t used(const std::string &uuid);
        }
    }
}