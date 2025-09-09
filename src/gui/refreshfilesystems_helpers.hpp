#pragma once
#include <QTableWidget>
#include <QPushButton>
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QString>
#include <functional>

namespace refresh_fs_helpers {

// -------------------------
// Build map UUID -> row
// -------------------------
QMap<QString,int> build_current_uuid_map(QTableWidget *fs_table);

// -------------------------
// Update existing rows or insert new ones
// -------------------------
void
update_or_insert_rows(QTableWidget *fs_table,
                      const std::vector<std::map<std::string,std::string>> &filesystems,
                      const QMap<QString,int> &current_uuid_map,
                      QSet<QString> &incoming_uuids,
                      const QMap<QString, QString> &uuid_status_map);

// -------------------------
// Remove rows that vanished
// -------------------------
void remove_vanished_rows(QTableWidget *fs_table,
                          const QSet<QString> &incoming_uuids);

// -------------------------
// Update button states
// -------------------------
void update_button_states(QTableWidget *fs_table,
                          QPushButton *start_btn,
                          QPushButton *stop_btn,
                          QPushButton *setup_btn,
                          std::function<void()> update_remove_btn);

// -------------------------
// Update status manager (dedup lines)
// -------------------------
void update_status_manager(QTableWidget *fs_table,
                           class DedupStatusManager &statusManager);

// -------------------------
// Read starting free space from file
// -------------------------
qint64 read_starting_free_space(const QString &uuid);

// -------------------------
// Utilities
// -------------------------
QString trim_config_path_after_colon(const std::string &cfg);

class status_text_mapper : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    QString map_status_text(const QString &status);
    QString map_status_manager_text(const qint64 starting_free, const qint64 free_bytes);
};


} // namespace refresh_fs_helpers

