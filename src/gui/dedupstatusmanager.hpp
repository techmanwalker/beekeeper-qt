#pragma once
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

// Class to store and retrieve dedup status messages per filesystem UUID
class DedupStatusManager : public QObject
{
    Q_OBJECT
public:
    void set_status(const QString &uuid, const QString &message);
    QString get_status(const QString &hovered_uuid) const;

signals:
    void status_updated(const QString &message);

private:
    QMap<QString, QString> status_map;
};
