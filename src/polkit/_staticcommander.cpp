#include "_staticcommander.hpp"
#include "beekeeper/supercommander.hpp"
#include "globals.hpp"

namespace beekeeper { namespace privileged { namespace _static {

QFuture<fs_map> btrfsls() { return komander->btrfsls(); }
QFuture<std::string> beesstatus(const QString &uuid) { return komander->beesstatus(uuid); }
QFuture<bool> beesstart(const QString &uuid, bool enable_logging) { return komander->beesstart(uuid, enable_logging); }
QFuture<bool> beesstop(const QString &uuid) { return komander->beesstop(uuid); }
QFuture<bool> beesrestart(const QString &uuid) { return komander->beesrestart(uuid); }
QFuture<std::string> beeslog(const QString &uuid) { return komander->beeslog(uuid); }
QFuture<bool> beesclean(const QString &uuid) { return komander->beesclean(uuid); }
QFuture<std::string> beessetup(const QString &uuid,
                                size_t db_size,
                                bool return_success_bool_instead) { return komander->beessetup(uuid, db_size, return_success_bool_instead); }
QFuture<std::string> beeslocate(const QString &uuid) { return komander->beeslocate(uuid); }
QFuture<bool> beesremoveconfig(const QString &uuid) { return komander->beesremoveconfig(uuid); }
QFuture<std::string> btrfstat(const QString &uuid, const QString &mode) { return komander->btrfstat(uuid, mode); }

QFuture<bool> add_uuid_to_autostart(const QString &uuid) { return komander->add_uuid_to_autostart(uuid); }
QFuture<bool> remove_uuid_from_autostart(const QString &uuid) { return komander->remove_uuid_from_autostart(uuid); }

QFuture<bool> add_uuid_to_transparentcompression(const QString &uuid, const QString &compression_token)
{
    return komander->add_uuid_to_transparentcompression(uuid, compression_token);
}
QFuture<bool> remove_uuid_from_transparentcompression(const QString &uuid)
{
    return komander->remove_uuid_from_transparentcompression(uuid);
}
QFuture<bool> start_transparentcompression_for_uuid(const QString &uuid)
{
    return komander->start_transparentcompression_for_uuid(uuid);
}
QFuture<bool> pause_transparentcompression_for_uuid(const QString &uuid)
{
    return komander->pause_transparentcompression_for_uuid(uuid);
}

}}} // namespace beekeeper::privileged::_static
