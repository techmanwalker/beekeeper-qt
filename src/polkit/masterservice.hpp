#pragma once

#include "beekeeper/util.hpp"
#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QStringList>
#include <QThreadPool>

#include <map>
#include <qcontainerfwd.h>

class masterservice : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.beekeeper.Helper")

public:
    explicit masterservice(QObject *parent = nullptr);
    ~masterservice();


    static std::map<std::string, std::string>
    convert_options(const QVariantMap &options);

    static std::vector<std::string>
    convert_subjects(const QStringList &subjects);

public slots:
    command_streams
    _internal_execute_clause(const QString &verb,
                const QVariantMap &options,
                const QStringList &subjects);

    QVariantMap
    execute_clause (const QString &verb,
                    const QVariantMap &options,
                    const QStringList &subjects);

private:
    QThreadPool worker_pool;
};