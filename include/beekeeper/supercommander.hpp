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

#include "beekeeper/internalaliases.hpp"
#include "beekeeper/util.hpp"
#include <string>
#include <memory>
#include <QDBusInterface>
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

    // call the helper to do something: wrapper
    QFuture<command_streams>
    call_bk_future(const QString &verb,
                               const QVariantMap &options,
                               const QStringList &subjects);
                
    bool do_i_have_root_permissions();

    // High-level wrappers...
    QFuture<fs_map> btrfsls();
    QFuture<std::string> beesstatus(const QString &uuid);
    QFuture<bool> beesstart(const QString &uuid, bool enable_logging = false);
    QFuture<bool> beesstop(const QString &uuid);
    QFuture<bool> beesrestart(const QString &uuid);
    QFuture<std::string> beeslog(const QString &uuid);
    QFuture<bool> beesclean(const QString &uuid);
    QFuture<std::string> beessetup(const QString &uuid,
                                    size_t db_size = 0,
                                    bool return_success_bool_instead = false);
    QFuture<std::string> beeslocate(const QString &uuid);
    QFuture<bool> beesremoveconfig(const QString &uuid);
    QFuture<std::string> btrfstat(const QString &uuid, const QString &mode = "");

    // Autostart control
    QFuture<bool> add_uuid_to_autostart(const QString &uuid);
    QFuture<bool> remove_uuid_from_autostart(const QString &uuid);

    // Transparent compression control
    QFuture<bool> add_uuid_to_transparentcompression(const QString &uuid, const QString &compression_token = "compress=lzo");
    QFuture<bool> remove_uuid_from_transparentcompression(const QString &uuid);

    QFuture<bool> start_transparentcompression_for_uuid(const QString &uuid);
    QFuture<bool> pause_transparentcompression_for_uuid(const QString &uuid);

signals:
    void command_finished(const QString &cmd,
                          const QString &stdout_str,
                          const QString &stderr_str);
};

} // namespace privileged
} // namespace beekeeper
