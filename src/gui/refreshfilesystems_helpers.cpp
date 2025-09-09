#include "dedupstatusmanager.hpp"
#include "refreshfilesystems_helpers.hpp"
#include "beekeeper/util.hpp"
#include "../polkit/globals.hpp"
#include "mainwindow.hpp"
#include <filesystem>
#include <fstream>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QTableWidgetItem>

namespace fs = std::filesystem;

namespace refresh_fs_helpers {

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
QMap<QString,int>
build_current_uuid_map(QTableWidget *fs_table)
{
    QMap<QString,int> map;
    for (int r = 0; r < fs_table->rowCount(); ++r) {
        QTableWidgetItem *item = fs_table->item(r, 0);
        if (!item) continue;
        QString uuid = item->data(Qt::UserRole).toString();
        if (!uuid.isEmpty()) map[uuid] = r;
    }
    return map;
}

// -------------------------
void
update_or_insert_rows(QTableWidget *fs_table,
                      const std::vector<std::map<std::string,std::string>> &filesystems,
                      const QMap<QString,int> &current_uuid_map,
                      QSet<QString> &incoming_uuids,
                      const QMap<QString, QString> &uuid_status_map)
{
    // To be able to map status texts to their full name and translate them
    refresh_fs_helpers::status_text_mapper mapper;

    for (const auto &fs : filesystems) {
        QString uuid = QString::fromStdString(fs.at("uuid"));
        incoming_uuids.insert(uuid);
        QString label = QString::fromStdString(fs.at("label"));

        QString status = QString::fromStdString(bk_util::trim_string(uuid_status_map.value(uuid, "unconfigured").toStdString()));

        QString display_status = mapper.map_status_text(status);

        if (current_uuid_map.contains(uuid)) {
            int row = current_uuid_map[uuid];
            QTableWidgetItem *name_item = fs_table->item(row, 1);
            if (name_item && name_item->text() != label)
                name_item->setText(QString::fromStdString(bk_util::trim_string(label.toStdString())));

            QTableWidgetItem *status_item = fs_table->item(row, 2);
            if (status_item) {
                status_item->setData(Qt::UserRole, status);
                status_item->setData(Qt::DisplayRole, display_status);
                if (status_item->text() != display_status)
                    status_item->setText(display_status);
            } else {
                status_item = new QTableWidgetItem(display_status);
                status_item->setData(Qt::UserRole, status);
                status_item->setData(Qt::DisplayRole, display_status);
                status_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                fs_table->setItem(row, 2, status_item);
            }
        } else {
            int row = fs_table->rowCount();
            fs_table->insertRow(row);

            QTableWidgetItem *uuid_item = new QTableWidgetItem();
            uuid_item->setData(Qt::UserRole, uuid);
            uuid_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            QTableWidgetItem *name_item = new QTableWidgetItem(label);
            name_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            QTableWidgetItem *status_item = new QTableWidgetItem(display_status);
            status_item->setData(Qt::UserRole, status);
            status_item->setData(Qt::DisplayRole, display_status);
            status_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            fs_table->setItem(row, 0, uuid_item);
            fs_table->setItem(row, 1, name_item);
            fs_table->setItem(row, 2, status_item);
        }
    }
}

// -------------------------
void
remove_vanished_rows(QTableWidget *fs_table,
                          const QSet<QString> &incoming_uuids)
{
    for (int row = fs_table->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *item = fs_table->item(row, 0);
        if (!item) continue;
        QString uuid = item->data(Qt::UserRole).toString();
        if (!incoming_uuids.contains(uuid))
            fs_table->removeRow(row);
    }
}

// -------------------------
void
update_button_states(QTableWidget *fs_table,
                          QPushButton *start_btn,
                          QPushButton *stop_btn,
                          QPushButton *setup_btn,
                          std::function<void()> update_remove_btn)
{
    auto selected_rows = fs_table->selectionModel()->selectedRows();
    QList<int> rows_to_check;
    if (selected_rows.isEmpty()) {
        for (int r = 0; r < fs_table->rowCount(); ++r) rows_to_check.append(r);
    } else {
        for (auto idx : selected_rows) rows_to_check.append(idx.row());
    }

    bool any_stopped = false, any_running = false, any_unconfigured = false;
    for (int row : rows_to_check) {
        QString status = fs_table->item(row, 2)->data(Qt::UserRole).toString().toLower();
        if (status.startsWith("running")) any_running = true;
        else if (status == "stopped") any_stopped = true;
        else if (status == "unconfigured") any_unconfigured = true;
    }

    start_btn->setEnabled(any_stopped);
    stop_btn->setEnabled(any_running);
    setup_btn->setEnabled(any_unconfigured);
    update_remove_btn();
}

// -------------------------
void
update_status_manager(QTableWidget *fs_table,
                      DedupStatusManager &statusManager)
{
    // To be able to map status texts to their full name and translate them
    refresh_fs_helpers::status_text_mapper mapper;

    for (int row = 0; row < fs_table->rowCount(); ++row) {
        QString uuid = fs_table->item(row, 0)->data(Qt::UserRole).toString();
        QString status_raw = fs_table->item(row, 2)->data(Qt::UserRole).toString();

        fs::path start_file = fs::path("/tmp") / (".beekeeper-" + uuid.toStdString()) / "startingfreespace";

        // query current free
        std::string free_str = komander->btrfstat(uuid.toStdString(), "free");
        qint64 free_bytes = QString::fromStdString(free_str).toLongLong();

        QString line;

        if (!fs::exists(start_file)) {
            line = mapper.map_status_manager_text(0, free_bytes);
            statusManager.set_status(uuid, line);
        }

        // load starting free space
        std::ifstream in(start_file);
        qint64 starting_free = 0;
        in >> starting_free;

        // build line
        line = mapper.map_status_manager_text(starting_free, free_bytes);

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

// -------------------------
QString
trim_config_path_after_colon(const std::string &cfg)
{
    auto pos = cfg.find(':');
    if (pos == std::string::npos) return QString::fromStdString(cfg);
    return QString::fromStdString(cfg.substr(0, pos));
}

} // namespace refresh_fs_helpers
