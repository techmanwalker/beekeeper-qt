#include "beekeeper/btrfsetup.hpp"
#include <iostream>

int main () {
    auto filesystems = bk_mgmt::btrfsls();
    
    for (const auto &[uuid, info] : filesystems) {
        std::cout << "Filesystem:\n";
        std::cout << "  uuid: " << uuid << "\n";
        std::cout << "  label: " << info.label << "\n";
        std::cout << "  status: " << info.status << "\n";
        std::cout << "  devname: " << info.devname << "\n";
    }
    
    return 0;
}