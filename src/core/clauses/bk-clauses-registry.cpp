#include "bk-clauses.hpp"
#include <unordered_map>
#include <string>

#include <QCoreApplication>  // Required for tr()

namespace clauses = beekeeper::clauses;

const std::unordered_map<std::string, clause>&
clauses_registry::get() {
    static std::unordered_map<std::string, clause> clauses_registry = {
        {
            "start",
            { 
                clauses::start,
                {
                    {"enable-logging", "l", false}
                },
                tr("UUID").toStdString(),
                tr("Start beesd daemon").toStdString(),
                1, -1
            }
        },
        {
            "stop", 
            { 
                clauses::stop,
                {},
                tr("UUID").toStdString(),
                tr("Stop beesd daemon").toStdString(),
                1, -1
            }
        },
        {
            "restart",
            {
                clauses::restart,
                {},
                tr("UUID").toStdString(),
                tr("Restart beesd daemon").toStdString(),
                1, -1
            }
        },
        {
            "status",
            {
                clauses::status,
                {},
                tr("UUID").toStdString(),
                tr("Check beesd status").toStdString(),
                1, -1
            }
        },
        {
            "log", 
            {
                clauses::log,
                {},
                tr("UUID").toStdString(),
                tr("Show log file").toStdString(),
                1, -1
            }
        },
        {
            "clean", 
            {
                clauses::clean,
                {},
                tr("UUID").toStdString(),
                tr("Clean PID file").toStdString(),
                1, -1
            }
        },
        {
            "setup",
            {
                clauses::setup,
                {
                    {"db-size", "d", true},
                    {"remove", "r", false},
                    {"json", "j", false}
                },
                tr("UUID").toStdString(),
                tr("Create/update configuration for a btrfs filesystem").toStdString(),
                1, 1
            }
        },
        {
            "locate",
            {
                clauses::locate,
                {
                    {"json", "j", false}
                },
                tr("UUID").toStdString(),
                tr("Show the mountpoints of a btrfs filesystem by UUID").toStdString(),
                1, -1
            }
        },
        {
            "list",
            {
                clauses::list,
                { {"json", "j", false} },
                tr("").toStdString(),
                tr("List available btrfs filesystems").toStdString(),
                0, 0
            }
        },
        {
            "stat",
            {
                clauses::stat,
                { {"storage", "s", true}, {"json", "j", false} },
                tr("UUID").toStdString(),
                tr("Check if a btrfs filesystem has a configuration").toStdString(),
                1, 1
            }
        },
        {
            "autostartctl",
            {
                clauses::autostartctl,
                { {"add", "a", false}, {"remove", "r", false} },
                tr("").toStdString(),
                tr("Add or remove filesystems from the autostart file").toStdString(),
                1, -1
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
                tr("").toStdString(),
                tr("Manage transparent compression (start, pause, status, add, or remove) on filesystems.\n"
                    "Options --algorithm / --algo and --level override presets given by --compression-level.").toStdString(),
                1, -1
            }
        },
        {
            "help",
            {
                clauses::help,
                {},
                tr("").toStdString(),
                tr("Show help information").toStdString(),
                0, 0
            }
        }
    };
    return clauses_registry;
}