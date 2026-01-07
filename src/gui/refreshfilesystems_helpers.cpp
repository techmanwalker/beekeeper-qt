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
#include <qtablewidget.h>

namespace fs = std::filesystem;

using mapper = refresh_fs_helpers::status_text_mapper;

namespace refresh_fs_helpers {

// Unified start-file path used everywhere
static fs::path startfile_for_uuid(const QString &uuid) {
    return fs::path("/tmp") / ".beekeeper" / uuid.toStdString() / "startingfreespace";
}

// -------------------------
// Internal helper: parse btrfstat output (robust)
// -------------------------
qint64
parse_free_bytes_from_btrfstat(const std::string &s)
{
    if (s.empty()) return 0;

    // First try direct conversion (if btrfstat returns raw bytes)
    try {
        return std::stoll(s);
    } catch (...) {
        // Fall back to parsing if needed
    }

    // trim whitespace
    size_t start = 0;
    while (start < s.size() && isspace((unsigned char)s[start])) ++start;
    size_t end = s.size();
    while (end > start && isspace((unsigned char)s[end-1])) --end;
    if (start >= end) return 0;
    std::string trimmed = s.substr(start, end-start);

    // Some implementations may return human-readable strings; we expect a number of bytes.
    // Try to parse leading integer number.
    try {
        size_t idx = 0;
        long long v = std::stoll(trimmed, &idx);
        if (idx == 0) return 0; // nothing parsed
        return static_cast<qint64>(v);
    } catch (...) {
        // not an integer: try to extract digits
        std::string digits;
        for (char c : trimmed) if (isdigit((unsigned char)c)) digits.push_back(c);
        if (digits.empty()) return 0;
        try { return static_cast<qint64>(std::stoll(digits)); } catch(...) { return 0; }
    }
}

// Return the number of rows actually selected in the table
int selected_rows_count(QTableWidget *table)
{
    if (!table) return 0;
    const auto sel = table->selectionModel()->selectedRows();
    return static_cast<int>(sel.size());
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
void update_status_manager(QTableWidget *fs_table,
                           DedupStatusManager &statusManager)
{
    if (!fs_table) return;

    for (int row = 0; row < fs_table->rowCount(); ++row) {
        auto *item_uuid = fs_table->item(row, 0);
        auto *item_status_raw = fs_table->item(row, 2);
        if (!item_uuid) continue;

        QString uuid = item_uuid->data(Qt::UserRole).toString();
        if (uuid.isEmpty()) continue;

        // Query current free using provided callable
        std::string free_str;
        try {
            free_str = komander->btrfstat(uuid.toStdString(), "free");
        } catch (...) {
            free_str.clear();
        }
        qint64 free_bytes = parse_free_bytes_from_btrfstat(free_str);

        // Read starting free (if present)
        qint64 starting_free = read_starting_free_space(uuid);

        // Build the status line
        QString line;
        if (starting_free > 0) {
            line = status_text_mapper().map_status_manager_text(starting_free, free_bytes);
        } else {
            line = status_text_mapper().map_status_manager_text(0, free_bytes);
        }

        statusManager.set_status(uuid, line);
    }
}

// -------------------------
// Fast path: only update the single hovered uuid so UI can show fresh info quickly
void update_status_manager_one_uuid(QTableWidget *fs_table,
                                    DedupStatusManager &statusManager,
                                    const QString &uuid)
{
    if (!fs_table || uuid.isEmpty()) return;

    // find the row for this uuid (cheap; usually small table)
    int row_found = -1;
    for (int row = 0; row < fs_table->rowCount(); ++row) {
        QTableWidgetItem *it = fs_table->item(row, 0);
        if (!it) continue;
        if (it->data(Qt::UserRole).toString() == uuid) {
            row_found = row;
            break;
        }
    }
    if (row_found < 0) return;

    // Query current free using callable
    std::string free_str;
    try {
        free_str = komander->btrfstat(uuid.toStdString(), "free");
    } catch (...) {
        free_str.clear();
    }
    qint64 free_bytes = parse_free_bytes_from_btrfstat(free_str);

    qint64 starting_free = read_starting_free_space(uuid);

    QString line;
    if (starting_free > 0) {
        line = status_text_mapper().map_status_manager_text(starting_free, free_bytes);
    } else {
        line = status_text_mapper().map_status_manager_text(0, free_bytes);
    }

    statusManager.set_status(uuid, line);
}

// -------------------------
qint64 read_starting_free_space(const QString &uuid)
{
    fs::path start_file = startfile_for_uuid(uuid);
    if (!fs::exists(start_file)) return 0;

    std::ifstream ifs(start_file);
    if (!ifs.is_open()) return 0;

    std::string line;
    std::getline(ifs, line);
    if (line.empty()) return 0;

    try {
        // stoll will throw on invalid input
        return std::stoll(line);
    } catch (...) {
        return 0;
    }
}

QString
fetch_user_role(
    const QModelIndex &idx, int column
)
{
    QModelIndex col_idx = idx.sibling(idx.row(), column);
    return col_idx.data(Qt::UserRole).toString();
}

} // namespace refresh_fs_helpers