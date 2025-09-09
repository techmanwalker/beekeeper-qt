#include "dedupstatusmanager.hpp"

void
DedupStatusManager::set_status(const QString &uuid, const QString &message)
{
    // Emit only if changed to avoid noisy re-emit during refresh
    auto it = status_map.find(uuid);
    if (it != status_map.end() && it.value() == message) {
        // unchanged → no emit
        return;
    }
    status_map[uuid] = message;
    emit status_updated(message);
}

QString
DedupStatusManager::get_status(const QString &hovered_uuid) const
{
    if (!hovered_uuid.isEmpty() && status_map.contains(hovered_uuid))
        return status_map.value(hovered_uuid);

    return QString(); // fallback empty
}
