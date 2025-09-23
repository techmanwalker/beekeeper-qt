#include "dedupstatusmanager.hpp"
#include "refreshfilesystems_helpers.hpp"
#include "beekeeper/util.hpp"
#include "../polkit/globals.hpp"
#include <filesystem>
#include <fstream>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QTableWidgetItem>

namespace fs = std::filesystem;

using mapper = refresh_fs_helpers::status_text_mapper;

namespace refresh_fs_helpers {

    // Return the number of rows actually selected in the table
int
selected_rows_count(QTableWidget *table)
{
    if (!table) return 0;
    return table->selectionModel()->selectedRows().size();
}

// Helper to map raw status (lowercase trimmed) to UI text
QString
status_text_mapper::map_status_text(const QString &status)
{
    QString s = status.trimmed().toLower();
    if (s.startsWith("running")) return tr("Deduplicating files");
    if (s == "stopped") return tr("Not running");
    if (s == "failed") return tr("Failed to run");
    if (s == "unconfigured") return tr("Not configured");
    return status;
}

// Helper to translate the status bar text
QString
status_text_mapper::map_status_manager_text(const qint64 starting_free, const qint64 free_bytes)
{
    QString line;
    if (starting_free > 0) {
        line = QString(tr("Deduplicating files. Started with %1 free, now you have %2 free."))
                    .arg(QString::fromStdString(bk_util::auto_size_suffix(starting_free)))
                    .arg(QString::fromStdString(bk_util::auto_size_suffix(free_bytes)));
    } else {
        line = QString(tr("Deduplicating files. You have %1 free right now."))
                    .arg(QString::fromStdString(bk_util::auto_size_suffix(free_bytes)));
    }

    return line;
}

// -------------------------
void
update_status_manager(QTableWidget *fs_table,
                      DedupStatusManager &statusManager)
{
    for (int row = 0; row < fs_table->rowCount(); ++row) {
        QString uuid = fs_table->item(row, 0)->data(Qt::UserRole).toString();
        QString status_raw = fs_table->item(row, 2)->data(Qt::UserRole).toString();

        fs::path start_file = fs::path("/tmp") / ".beekeeper" / uuid.toStdString() / "startingfreespace";

        // query current free
        std::string free_str = komander->btrfstat(uuid.toStdString(), "free");
        qint64 free_bytes = QString::fromStdString(free_str).toLongLong();

        QString line;

        if (!fs::exists(start_file)) {
            line = mapper().map_status_manager_text(0, free_bytes);
            statusManager.set_status(uuid, line);
        }

        // load starting free space
        std::ifstream in(start_file);
        qint64 starting_free = 0;
        in >> starting_free;

        // build line
        line = mapper().map_status_manager_text(starting_free, free_bytes);

        // cache in manager (works both running and stopped)
        statusManager.set_status(uuid, line);
    }
}

// -------------------------
qint64
read_starting_free_space(const QString &uuid)
{
    fs::path start_file = fs::path("/tmp") / (".beekeeper-" + uuid.toStdString()) / "startingfreespace";
    if (!fs::exists(start_file)) return 0;

    std::ifstream ifs(start_file);
    if (!ifs.is_open()) return 0;

    std::string line;
    std::getline(ifs, line);
    if (line.empty()) return 0;

    try { return std::stoll(line); } catch(...) { return 0; }
}

} // namespace refresh_fs_helpers