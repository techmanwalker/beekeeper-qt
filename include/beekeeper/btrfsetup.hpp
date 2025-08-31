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
    }
}