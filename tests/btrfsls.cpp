#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/internalaliases.hpp"
#include <iostream>

int main () {
    auto filesystems = bk_mgmt::btrfsls();
    
    for (const auto& fs : filesystems) {
        std::cout << "Filesystem:\n";
        std::cout << "  label: " << fs.at("label") << "\n";
        std::cout << "  uuid: " << fs.at("uuid") << "\n";
    }
    
    return 0;
}