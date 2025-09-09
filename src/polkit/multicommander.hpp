#pragma once
#include <QObject>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <cstddef>
#include "beekeeper/supercommander.hpp"

// supercommander, but async
namespace beekeeper { namespace privileged {

class multicommander : public beekeeper::privileged::supercommander
{
    Q_OBJECT

public:
    using supercommander::supercommander; // inherit constructors

    // Async wrappers for submethods
    QFuture<std::vector<std::map<std::string,std::string>>> btrfsls();
    QFuture<std::string> beesstatus(const QString &uuid);
    QFuture<bool> beesstart(const QString &uuid);
    QFuture<bool> beesstop(const QString &uuid);
    QFuture<bool> beesrestart(const QString &uuid);
    QFuture<std::string> beeslog(const QString &uuid);
    QFuture<bool> beesclean(const QString &uuid);
    QFuture<std::string> beessetup(const QString &uuid, size_t db_size);
    QFuture<std::string> beeslocate(const QString &uuid);
    QFuture<bool> beesremoveconfig(const QString &uuid);
    QFuture<std::string> btrfstat(const QString &uuid, const QString &mode /* = "free" */);
};

} // namespace privileged
} // namespace beekeeper