#include "../include/beekeeper/btrfsetup.hpp"
#include <iostream>

int main (int argc, char **argv) {
    std::cout << "What's my device's UUID? " << argv[1] << std::endl;
    std::string config_path = bk_mgmt::btrfstat(argv[1]);
    std::cout << "Is it configured? " << (!(config_path == "") ? "yes" : "no") << std::endl;
    
    if (!(config_path == "")) {
        std::cout << "Config file path: " << config_path << std::endl;
    } else {
        std::cout << "Setting up automaticatically with btrfsetup()..." << std::endl;
        bk_mgmt::beessetup(argv[1]);
    }
}