#include "multicommander.hpp"
#include "globals.hpp"
#include "beekeeper/supercommander.hpp"
#include <cstddef>
using namespace beekeeper::privileged; // simple qualifier as requested

QFuture<std::vector<std::map<std::string,std::string>>>
multicommander::btrfsls()
{
    return QtConcurrent::run([this]{
        auto result = komander->btrfsls();
        emit command_finished("btrfsls", "success", "");
        return result;
    });
}

QFuture<std::string>
multicommander::beesstatus(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->beesstatus(uuid.toStdString());
        emit command_finished("beesstatus", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::beesstart(const QString &uuid, bool enable_logging)
{
    return QtConcurrent::run([this, uuid, enable_logging]{
        auto result = komander->beesstart(uuid.toStdString(), enable_logging);
        emit command_finished("beesstart", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<bool>
multicommander::beesstop(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->beesstop(uuid.toStdString());
        emit command_finished("beesstop", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<bool>
multicommander::beesrestart(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->beesrestart(uuid.toStdString());
        emit command_finished("beesrestart", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<std::string>
multicommander::beeslog(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->beeslog(uuid.toStdString());
        emit command_finished("beeslog", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::beesclean(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->beesclean(uuid.toStdString());
        emit command_finished("beesclean", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<std::string>
multicommander::beessetup(const QString &uuid, size_t db_size)
{
    return QtConcurrent::run([this, uuid, db_size]{
        auto result = komander->beessetup(uuid.toStdString(), db_size);
        emit command_finished("beessetup", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<std::string>
multicommander::beeslocate(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]() -> std::string {
        auto result = komander->beeslocate(uuid.toStdString());
        emit command_finished("beeslocate", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::beesremoveconfig(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->beesremoveconfig(uuid.toStdString());
        emit command_finished("beesremoveconfig", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<std::string>
multicommander::btrfstat(const QString &uuid, const QString &mode /* = "free" */)
{
    return QtConcurrent::run([this, uuid, mode]{
        auto result = komander->btrfstat(uuid.toStdString(),
                                                     mode.toStdString());
        emit command_finished("btrfstat", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::add_uuid_to_autostart(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->add_uuid_to_autostart(uuid.toStdString());
        emit command_finished("autostartctl:add", "success", "");
        return result;
    });
}

QFuture<bool>
multicommander::remove_uuid_from_autostart(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = komander->remove_uuid_from_autostart(uuid.toStdString());
        emit command_finished("autostartctl:remove", "success", "");
        return result;
    });
}
