#include "multicommander.hpp"
#include <cstddef>
using namespace beekeeper::privileged; // simple qualifier as requested

QFuture<std::vector<std::map<std::string,std::string>>>
multicommander::btrfsls()
{
    return QtConcurrent::run([this]{
        auto result = this->supercommander::btrfsls();
        emit command_finished("btrfsls", "success", "");
        return result;
    });
}

QFuture<std::string>
multicommander::beesstatus(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beesstatus(uuid.toStdString());
        emit command_finished("beesstatus", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::beesstart(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beesstart(uuid.toStdString());
        emit command_finished("beesstart", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<bool>
multicommander::beesstop(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beesstop(uuid.toStdString());
        emit command_finished("beesstop", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<bool>
multicommander::beesrestart(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beesrestart(uuid.toStdString());
        emit command_finished("beesrestart", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<std::string>
multicommander::beeslog(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beeslog(uuid.toStdString());
        emit command_finished("beeslog", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::beesclean(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beesclean(uuid.toStdString());
        emit command_finished("beesclean", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<std::string>
multicommander::beessetup(const QString &uuid, size_t db_size)
{
    return QtConcurrent::run([this, uuid, db_size]{
        auto result = this->supercommander::beessetup(uuid.toStdString(), db_size);
        emit command_finished("beessetup", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<std::string>
multicommander::beeslocate(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]() -> std::string {
        auto result = this->supercommander::beeslocate(uuid.toStdString());
        emit command_finished("beeslocate", QString::fromStdString(result), "");
        return result;
    });
}

QFuture<bool>
multicommander::beesremoveconfig(const QString &uuid)
{
    return QtConcurrent::run([this, uuid]{
        auto result = this->supercommander::beesremoveconfig(uuid.toStdString());
        emit command_finished("beesremoveconfig", (result ? "success" : ""), (result ? "" : "fail"));
        return result;
    });
}

QFuture<std::string>
multicommander::btrfstat(const QString &uuid, const QString &mode /* = "free" */)
{
    return QtConcurrent::run([this, uuid, mode]{
        auto result = this->supercommander::btrfstat(uuid.toStdString(),
                                                     mode.toStdString());
        emit command_finished("btrfstat", QString::fromStdString(result), "");
        return result;
    });
}
