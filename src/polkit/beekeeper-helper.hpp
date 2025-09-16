#pragma once

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QStringList>

// Forward declaration of helper utility
struct command_streams;

class HelperObject : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.beekeeper.Helper")

public:
    HelperObject(QObject *parent = nullptr);

private:
    void run_autostart_tasks();

public slots:
    // ExecuteCommand: (s, a{ss}, as) -> a{ss}
    QVariantMap ExecuteCommand(const QString &verb, const QVariantMap &options, const QStringList &subjects);
};
