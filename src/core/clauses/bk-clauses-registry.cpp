#include "bk-clauses.hpp"

namespace clauses = beekeeper::clauses;

std::unordered_map<std::string, clause> clauses_registry = {
    {
        "start",
        { 
            clauses::start,
            {
                {"enable-logging", "l", false}  // long, short, requires_value
            },
            "UUID",
            "Start beesd daemon",
            1, -1
        }
    },
    {
        "stop", 
        { 
            clauses::stop,
            {},
            "UUID",
            "Stop beesd daemon",
            1, -1
        }
    },
    {
        "restart",
        {
            clauses::restart,
            {},
            "UUID",
            "Restart beesd daemon",
        1, -1
        }
    },
    {
        "status",
        {
            clauses::status,
            {},
            "UUID",
            "Check beesd status",
            1, -1
        }
    },
    {
        "log", 
        {
            clauses::log,
            {},
            "UUID",
            "Show log file",
            1, -1
        }
    },
    {
        "clean", 
        {
            clauses::clean,
            {},
            "UUID",
            "Clean PID file",
            1, -1
        }
    },
    {
        "setup",
        {
            clauses::setup,
            {
                {"db-size", "d", true},  // Requires value (size in bytes)
                {"remove", "r", false},
                {"json", "j", false}
            },
            "UUID",
            "Create/update configuration for a btrfs filesystem",
            1, 1
        }
    },
    {
        "locate",
        {
            clauses::locate,
            {
                {"json", "j", false} // for machine readability
            },
            "UUID",
            "Show the mountpoint of a btrfs filesystem by UUID",
            1, -1
        }
    },
    {
        "list",
        {
            clauses::list,
            { {"json", "j", false} }, // <-- support -j / --json
            "",
            "List available btrfs filesystems",
            0, 0
        }
    },
    {
        "stat",
        {
            clauses::stat,
            { {"storage", "s", true}, {"json", "j", false} }, // <-- support -s / --storage <value>
            "UUID",
            "Check if a btrfs filesystem has a configuration",
            1, 1
        }
    },
    {
        "autostartctl",
        {
            clauses::autostartctl,
            { {"add", "a", false}, {"remove", "r", false} }, // support --add / --remove
            "",
            "Add or remove filesystems from the autostart file",
            1, -1 // min 1 subject, max infinite
        }
    },
{
        "compressctl",
        {
            clauses::compressctl,
            {
                {"start", "s", false},
                {"pause", "p", false},
                {"status", "i", false},
                {"add", "a", false},
                {"remove", "r", false},
                {"compression-level", "c", false},
                {"algorithm", "", true},
                {"level", "", false},
                {"json", "j", false}
            },
            "",
            "Control transparent compression (start, pause, status, add, or remove) on filesystems.\n"
            "Options --algorithm / --algo and --level override presets given by --compression-level.",
            1, -1 // min 1 subject, max infinite
        }
    },
    {
        "help",
        {
            clauses::help,
            {},
            "",
            "Show help information",
            0, 0
        }
    }
};