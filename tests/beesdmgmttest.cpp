#include "../include/beekeeper/beesdmgmt.hpp"
#include "../include/beekeeper/internalaliases.hpp"
#include <iostream>

int
main (int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <start|stop|restart|status> <uuid>\n";
        return 1;
    }
    
    std::string command = argv[1];
    std::string uuid = argv[2];
    
    if (command == "start") {
        if (bk_mgmt::beesstart(uuid)) {
            std::cout << "Started beesd for " << uuid << std::endl;
        } else {
            std::cerr << "Failed to start beesd for " << uuid << std::endl;
        }
    }
    else if (command == "stop") {
        if (bk_mgmt::beesstop(uuid)) {
            std::cout << "Stopped beesd for " << uuid << std::endl;
        } else {
            std::cerr << "Failed to stop beesd for " << uuid << std::endl;
        }
    }
    else if (command == "restart") {
        if (bk_mgmt::beesrestart(uuid)) {
            std::cout << "Restarted beesd for " << uuid << std::endl;
        } else {
            std::cerr << "Failed to restart beesd for " << uuid << std::endl;
        }
    }
    else if (command == "status") {
        std::string status = bk_mgmt::beesstatus(uuid);
        std::cout << "Status: " << status << std::endl;
    }
    else {
        std::cerr << "Invalid command: " << command << std::endl;
        return 1;
    }
    
    return 0;
}