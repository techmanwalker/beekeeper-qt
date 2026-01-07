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
    QFuture<fs_map> btrfsls();
    QFuture<std::string> beesstatus(const QString &uuid);
    QFuture<bool> beesstart(const QString &uuid, bool enable_logging = false);
    QFuture<bool> beesstop(const QString &uuid);
    QFuture<bool> beesrestart(const QString &uuid);
    QFuture<std::string> beeslog(const QString &uuid);
    QFuture<bool> beesclean(const QString &uuid);
    QFuture<bool> beessetup(const QString &uuid, size_t db_size = 0);
    QFuture<std::string> beeslocate(const QString &uuid);
    QFuture<bool> beesremoveconfig(const QString &uuid);
    QFuture<bool> btrfstat(const QString &uuid, const QString &mode = "");
    QFuture<bool> add_uuid_to_transparentcompression(const QString &uuid, const QString &compression_level);
    QFuture<bool> remove_uuid_from_transparentcompression(const QString &uuid);
    QFuture<bool> start_transparentcompression_for_uuid(const QString &uuid);
    QFuture<bool> pause_transparentcompression_for_uuid(const QString &uuid);

    // Autostart control
    QFuture<bool> add_uuid_to_autostart(const QString &uuid);
    QFuture<bool> remove_uuid_from_autostart(const QString &uuid);

};

// static versions

namespace _static {
        // Async wrappers for submethods
    QFuture<fs_map> btrfsls();
    QFuture<std::string> beesstatus(const QString &uuid);
    QFuture<bool> beesstart(const QString &uuid, bool enable_logging = false);
    QFuture<bool> beesstop(const QString &uuid);
    QFuture<bool> beesrestart(const QString &uuid);
    QFuture<std::string> beeslog(const QString &uuid);
    QFuture<bool> beesclean(const QString &uuid);
    QFuture<bool> beessetup(const QString &uuid, size_t db_size = 0);
    QFuture<std::string> beeslocate(const QString &uuid);
    QFuture<bool> beesremoveconfig(const QString &uuid);
    QFuture<bool> btrfstat(const QString &uuid, const QString &mode = "");
    QFuture<bool> add_uuid_to_transparentcompression(const QString &uuid, const QString &compression_level);
    QFuture<bool> remove_uuid_from_transparentcompression(const QString &uuid);
    QFuture<bool> start_transparentcompression_for_uuid(const QString &uuid);
    QFuture<bool> pause_transparentcompression_for_uuid(const QString &uuid);

    // Autostart control
    QFuture<bool> add_uuid_to_autostart(const QString &uuid);
    QFuture<bool> remove_uuid_from_autostart(const QString &uuid);
}

} // namespace privileged
} // namespace beekeeper