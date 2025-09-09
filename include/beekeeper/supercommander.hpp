#pragma once

// include/beekeeper/supercommander.hpp
//
// SuperCommander — high-level, type-safe wrappers around the beekeeperman CLI.
// It uses a BeeKeeperPower (low-level exec wrapper) that already runs commands
// with the right privileges and *auto-prepends* "beekeeperman " to whatever
// command we pass.
//
// Responsibilities:
//   - Call BeeKeeperPower::execute_command_in_forked_shell("...") with
//     subcommands only (e.g. "list -j", "status <uuid>", etc.).
//   - Parse JSON output for commands that support it (Qt6 JSON).
//   - Return simple C++ types convenient for the GUI.
//
// Non-responsibilities:
//   - It does not manage Polkit or process lifetime (SuperLaunch does).
//   - It does not directly call any bk_mgmt::* code.
//
// JSON expectations (initial):
//   - `list -j` returns a JSON array of objects, e.g.
//       [ { "uuid": "...", "label": "foo", "config": "/etc/bees/uuid.conf" }, ... ]
//     Only "uuid" is required; others are optional.
//
// Notes:
//   - If JSON parsing fails in btrfsls(), an empty result is returned.
//   - For commands without JSON, we just return stdout trimmed (or bool on success).
//
// License: same as project.

class superlaunch;   // <--- forward declare so we can friend it later

#include "beekeeper/util.hpp"
#include "debug.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <QObject>
#include <QtConcurrent/QtConcurrent>

namespace beekeeper {
namespace privileged {

// forward declaration
class multicommander;

class supercommander : public QObject
{
    Q_OBJECT
    // allow the global superlaunch class to access internals if absolutely needed
    friend class ::superlaunch;

public:
    supercommander() = default;
    ~supercommander() = default;

    supercommander(const supercommander&) = delete;
    supercommander& operator=(const supercommander&) = delete;

    // call the helper to do something
    command_streams
    call_bk(const QString &verb,
            const QVariantMap &options,
            const QStringList &subjects);

    // Fully asynchronous call to the DBus helper
    QFuture<command_streams>
    call_bk_async(const QString &verb,
                const QVariantMap &options,
                const QStringList &subjects);

    bool do_i_have_root_permissions();

    // High-level wrappers...
    std::vector<std::map<std::string,std::string>> btrfsls();
    std::string beesstatus(const std::string &uuid);
    bool beesstart(const std::string &uuid, bool enable_logging = false);
    bool beesstop(const std::string &uuid);
    bool beesrestart(const std::string &uuid);
    std::string beeslog(const std::string &uuid);
    bool beesclean(const std::string &uuid);
    std::string beessetup(const std::string &uuid, size_t db_size = 0);
    std::string beeslocate(const std::string &uuid);
    bool beesremoveconfig(const std::string &uuid);
    std::string btrfstat(const std::string &uuid, const std::string &mode);

    // Async wrapper submethods
    std::unique_ptr<multicommander> async;

signals:
    void command_finished(const QString &cmd,
                          const QString &stdout_str,
                          const QString &stderr_str);
};

} // namespace privileged
} // namespace beekeeper
