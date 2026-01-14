#include "beekeeper/commandregistry.hpp"

namespace clause = beekeeper::clause;

std::unordered_map<std::string, cm::command> command_registry = {
    {
        "start",
        { 
            clause::start,
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
            clause::stop,
            {},
            "UUID",
            "Stop beesd daemon",
            1, -1
        }
    },
    {
        "restart",
        {
            clause::restart,
            {},
            "UUID",
            "Restart beesd daemon",
        1, -1
        }
    },
    {
        "status",
        {
            clause::status,
            {},
            "UUID",
            "Check beesd status",
            1, -1
        }
    },
    {
        "log", 
        {
            clause::log,
            {},
            "UUID",
            "Show log file",
            1, -1
        }
    },
    {
        "clean", 
        {
            clause::clean,
            {},
            "UUID",
            "Clean PID file",
            1, -1
        }
    },
    {
        "setup",
        {
            clause::setup,
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
            clause::locate,
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
            clause::list,
            { {"json", "j", false} }, // <-- support -j / --json
            "",
            "List available btrfs filesystems",
            0, 0
        }
    },
    {
        "stat",
        {
            clause::stat,
            { {"storage", "s", true}, {"json", "j", false} }, // <-- support -s / --storage <value>
            "UUID",
            "Check if a btrfs filesystem has a configuration",
            1, 1
        }
    },
    {
        "autostartctl",
        {
            clause::autostartctl,
            { {"add", "a", false}, {"remove", "r", false} }, // support --add / --remove
            "",
            "Add or remove filesystems from the autostart file",
            1, -1 // min 1 subject, max infinite
        }
    },
{
        "compressctl",
        {
            clause::compressctl,
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
            clause::help,
            {},
            "",
            "Show help information",
            0, 0
        }
    }
};