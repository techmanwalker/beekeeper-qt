#include "commandregistry.hpp"

std::vector<cm::command> command_registry = {
    {
        "start", 
        beekeeper::cli::handle_start,
        {
            {"enable-logging", "l", false}  // long, short, requires_value
        },
        "UUID",
        "Start beesd daemon",
        1, -1
    },
    {
        "stop", 
        beekeeper::cli::handle_stop,
        {},
        "UUID",
        "Stop beesd daemon",
        1, -1
    },
    {
        "restart", 
        beekeeper::cli::handle_restart,
        {},
        "UUID",
        "Restart beesd daemon",
        1, -1
    },
    {
        "status", 
        beekeeper::cli::handle_status,
        {},
        "UUID",
        "Check beesd status",
        1, -1
    },
    {
        "log", 
        beekeeper::cli::handle_log,
        {},
        "UUID",
        "Show log file",
        1, -1
    },
    {
        "clean", 
        beekeeper::cli::handle_clean,
        {},
        "UUID",
        "Clean PID file",
        1, -1
    },
    {
        "setup",
        beekeeper::cli::handle_setup,
        {
            {"db-size", "d", true},  // Requires value (size in bytes)
            {"remove", "r", false}
        },
        "UUID",
        "Create/update configuration for a btrfs filesystem",
        1, 1
    },
    {
        "locate",
        beekeeper::cli::handle_locate,
        {},
        "UUID",
        "Show the mountpoint of a btrfs filesystem by UUID",
        1, -1
    },
    {
        "list",
        beekeeper::cli::handle_list,
        { {"json", "j", false} }, // <-- support -j / --json
        "",
        "List available btrfs filesystems",
        0, 0
    },
    {
        "stat",
        beekeeper::cli::handle_stat,
        { {"storage", "s", true}, {"json", "j", false} }, // <-- support -s / --storage <value>
        "UUID",
        "Check if a btrfs filesystem has a configuration",
        1, 1
    },
    {
        "autostartctl",
        beekeeper::cli::handle_autostartctl,
        { {"add", "a", false}, {"remove", "r", false} }, // support --add / --remove
        "",
        "Add or remove filesystems from the autostart file",
        1, -1 // min 1 subject, max infinite
    },
    {
        "help", 
        beekeeper::cli::handle_help,
        {},
        "",
        "Show help information",
        0, 0
    }
};