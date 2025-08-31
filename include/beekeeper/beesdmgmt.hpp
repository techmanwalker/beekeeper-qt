#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

// Trunk program
namespace beekeeper {
    // Functions to manage an tame beesd
    namespace management {
        // Start beesd daemon for specified UUID
        bool
        beesstart (const std::string& uuid, bool enable_logging = false);

        // Stop beesd daemon for specified UUID
        bool
        beesstop (const std::string& uuid);

        // Restart beesd daemon for specified UUID
        bool
        beesrestart (const std::string& uuid);

        // Get running status
        // Returns: "running", "stopped", or "error"
        std::string
        beesstatus (const std::string& uuid);

        // Logging management -----

        // Start beesd in foreground for logging
        void
        beeslog (const std::string& uuid);

        // Logger management
        void
        beesstoplogger (const std::string& uuid);

        // Clean up PID file
        void
        beescleanlogfiles (const std::string& uuid);
    }
}