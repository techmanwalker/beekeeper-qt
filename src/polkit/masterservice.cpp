#include "masterservice.hpp"

#include "beekeeper/commandregistry.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"
#include "diskwait.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QRunnable>

#include <iostream>
#include <map>
#include <string>
#include <vector>

//
// ---------- Utilities (pure, thread-safe) ----------
//

std::map<std::string, std::string>
masterservice::convert_options(const QVariantMap &options)
{
    std::map<std::string, std::string> result;

    for (const auto &[key, value] : options.asKeyValueRange()) {
        result.emplace(key.toStdString(), value.toString().toStdString());
    }

    return result;
}

std::vector<std::string>
masterservice::convert_subjects(const QStringList &subjects)
{
    std::vector<std::string> result;

    for (const auto &subject : subjects) {
        result.emplace_back(subject.toStdString());
    }

    return result;
}

//
// ---------- Worker runnable ----------
//

struct clause_runnable : public QRunnable
{
    clause_runnable(const QDBusMessage &msg,
                    const QString &verb,
                    const QVariantMap &options,
                    const QStringList &subjects)
        : pending_msg(msg),
          verb(verb),
          options(options),
          subjects(subjects)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        command_streams reply;

        auto it = command_registry.find(verb.toStdString());
        if (it == command_registry.end()) {
            reply.errcode = 1;
            reply.stdout_str.clear();
            reply.stderr_str = "Unknown clause: " + verb.toStdString();
        } else {
            cm::command &cmd = it->second;
            reply = cmd.handler(
                masterservice::convert_options(options),
                masterservice::convert_subjects(subjects)
            );
        }

        QVariantMap dbus_reply;
        dbus_reply["stdout_str"] = QString::fromStdString(reply.stdout_str);
        dbus_reply["stderr_str"] = QString::fromStdString(reply.stderr_str);
        dbus_reply["errcode"]    = reply.errcode;

        // Resolve the original DBus promise
        QDBusConnection::systemBus().send(
            pending_msg.createReply(dbus_reply)
        );
    }

    QDBusMessage pending_msg;
    QString verb;
    QVariantMap options;
    QStringList subjects;
};

//
// ---------- masterservice ----------
//

masterservice::masterservice(QObject *parent)
    : QObject(parent)
{
    worker_pool.setMaxThreadCount(QThread::idealThreadCount());
}

masterservice::~masterservice() = default;

//
// This is a private pure backend executor (never touches DBus)
//
command_streams
masterservice::_internal_execute_clause(const QString &verb,
                                        const QVariantMap &options,
                                        const QStringList &subjects)
{
    auto it = command_registry.find(verb.toStdString());
    if (it == command_registry.end()) {
        return {
            "",
            "Unknown clause: " + verb.toStdString(),
            1
        };
    }

    return it->second.handler(
        convert_options(options),
        convert_subjects(subjects)
    );
}

//
// This is the DBus entrypoint
//
QVariantMap
masterservice::execute_clause(const QString &verb,
                              const QVariantMap &options,
                              const QStringList &subjects)
{
    // Capture the incoming DBus call
    QDBusMessage msg = message();

    // Tell Qt: do NOT auto-reply, we'll do it later
    setDelayedReply(true);

    // Fork into a worker thread
    worker_pool.start(new clause_runnable(msg, verb, options, subjects));

    // Return nothing now — DBus reply will be sent from worker
    return QVariantMap();
}

//
// ---------- main ----------
//

int
main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        std::cerr << "System D-Bus is not connected; aborting helper\n";
        return 1;
    }

    if (bus.interface()->isServiceRegistered("org.beekeeper.Helper")) {
        DEBUG_LOG("DBus name already owned, exiting cleanly");
        return 0;
    }

    if (!bus.registerService("org.beekeeper.Helper")) {
        std::cerr << "Failed to claim DBus name org.beekeeper.Helper\n";
        return 2;
    }

    DEBUG_LOG("[thebeekeeper] registered service org.beekeeper.Helper");

    masterservice helper;
    if (!bus.registerObject("/org/beekeeper/Helper", &helper,
                            QDBusConnection::ExportAllSlots)) {
        std::cerr << "Failed to register DBus object /org/beekeeper/Helper\n";
        return 1;
    }

    // diskwait is totally independent
    diskwait *disk_thread = new diskwait();
    disk_thread->start();
    DEBUG_LOG("[thebeekeeper] diskwait thread launched");

    DEBUG_LOG("[thebeekeeper] Helper DBus service ready and waiting for calls");
    return app.exec();
}
