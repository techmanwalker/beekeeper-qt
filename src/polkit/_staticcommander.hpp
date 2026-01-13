#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrent>
#include <string>
#include "beekeeper/internalaliases.hpp"  // fs_map, command_streams, fs_info, etc.

namespace beekeeper { namespace privileged { namespace _static {

// High-level beekeeperman wrappers
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
QFuture<bool> add_uuid_to_transparentcompression(const QString &uuid,
                                                  const QString &compression_token = "compress=lzo");
QFuture<bool> remove_uuid_from_transparentcompression(const QString &uuid);
QFuture<bool> start_transparentcompression_for_uuid(const QString &uuid);
QFuture<bool> pause_transparentcompression_for_uuid(const QString &uuid);

}}} // namespace beekeeper::privileged::_static
