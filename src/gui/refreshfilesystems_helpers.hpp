#pragma once
#include "dedupstatusmanager.hpp"
#include <QTableWidget>
#include <QPushButton>
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QString>

namespace refresh_fs_helpers {

// Return the number of rows actually selected in the table
int
selected_rows_count(QTableWidget *table);

// -------------------------
// Update status manager (dedup lines)
// -------------------------
    // Update status for all rows. btrfstat_func(uuid, "free") -> returns string free bytes
    void update_status_manager(QTableWidget *fs_table,
                               DedupStatusManager &statusManager);

    // Update status only for a single uuid (fast path for hover)
    void update_status_manager_one_uuid(QTableWidget *fs_table,
                                        DedupStatusManager &statusManager,
                                        const QString &uuid);

    // Read the starting free space file (returns 0 if missing / malformed)
    qint64 read_starting_free_space(const QString &uuid);

// -------------------------
// Utilities
// -------------------------
qint64
parse_free_bytes_from_btrfstat(const std::string &s);

QString
fetch_user_role(
    const QModelIndex &idx, int column
);

class status_text_mapper : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    QString map_status_text(const QString &status);
    QString map_status_manager_text(const qint64 starting_free, const qint64 free_bytes);
};


} // namespace refresh_fs_helpers

