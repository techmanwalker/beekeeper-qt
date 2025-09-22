#include "masterservice.hpp"
#include "../cli/handlers.hpp"
#include "beekeeper/internalaliases.hpp"
#include "../cli/commandregistry.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"
#include "diskwait.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <iostream>
#include <map>
#include <pwd.h>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <algorithm> // for find_if

masterservice::masterservice(QObject *parent)
    : QObject(parent)
{
    // No worker/thread here anymore — DBus calls are handled synchronously by this object.
}

// No special destructor required
masterservice::~masterservice()
{
}

// Converts QVariantMap -> std::map<std::string, std::string>
std::map<std::string, std::string>
masterservice::convert_options(const QVariantMap &options)
{
    std::map<std::string, std::string> out;
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        out[it.key().toStdString()] = it.value().toString().toStdString();
    }
    return out;
}

// Converts QStringList -> std::vector<std::string>
std::vector<std::string>
masterservice::convert_subjects(const QStringList &subjects)
{
    std::vector<std::string> out;
    out.reserve(subjects.size());
    for (const QString &s : subjects) {
        out.push_back(s.toStdString());
    }
    return out;
}

// Synchronous DBus slot: find handler, capture stdout/stderr, run handler and return result map
QVariantMap
masterservice::ExecuteCommand(const QString &verb,
                              const QVariantMap &options,
                              const QStringList &subjects)
{
    QVariantMap reply_map;
    reply_map.insert("stdout", QString());
    reply_map.insert("stderr", QString());

    DEBUG_LOG("[supercommander] call_bk: before DBus call for verb ",
              verb.toStdString() + " :" + bk_util::current_timestamp());

    std::string verb_std = verb.toStdString();
    std::map<std::string, std::string> opts_std = convert_options(options);
    std::vector<std::string> subs_std = convert_subjects(subjects);

    // Find handler in registry
    auto it = std::find_if(command_registry.begin(), command_registry.end(),
                           [&verb_std](const cm::command &cmd) {
                               return cmd.name == verb_std;
                           });

    if (it == command_registry.end()) {
        std::string err = "Unknown command verb: " + verb_std;
        DEBUG_LOG(err);
        reply_map["stderr"] = QString::fromStdString(err);
        DEBUG_LOG("[supercommander] call_bk: after DBus call for verb ",
                  verb.toStdString() + " :" + bk_util::current_timestamp());
        return reply_map;
    }

    // Capture stdout/stderr into buffers so the caller gets them in the QVariantMap
    std::stringstream cout_buf, cerr_buf;
    auto* old_cout = std::cout.rdbuf(cout_buf.rdbuf());
    auto* old_cerr = std::cerr.rdbuf(cerr_buf.rdbuf());

    // Execute the handler synchronously (same behavior as before)
    int ret = it->handler(opts_std, subs_std);

    // Restore original streams
    std::cout.rdbuf(old_cout);
    std::cerr.rdbuf(old_cerr);

    // Fill reply_map with captured output
    reply_map["stdout"] = QString::fromStdString(cout_buf.str());
    reply_map["stderr"] = QString::fromStdString(cerr_buf.str());

    if (ret != 0) {
        QString current_err = reply_map["stderr"].toString();
        current_err += QString("Handler returned non-zero exit code: %1\n").arg(ret);
        reply_map["stderr"] = current_err;
    }

    DEBUG_LOG("[supercommander] call_bk: after DBus call for verb ",
              verb.toStdString() + " :" + bk_util::current_timestamp());

    return reply_map;
}

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

    DEBUG_LOG("[beekeeper-helper] registered service org.beekeeper.Helper");

    masterservice helper;
    if (!bus.registerObject("/org/beekeeper/Helper", &helper,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        std::cerr << "Failed to register DBus object /org/beekeeper/Helper\n";
        return 1;
    }

    // Start diskwait thread after DBus registration (diskwait stays a separate QThread)
    diskwait *disk_thread = new diskwait();
    disk_thread->start();
    DEBUG_LOG("[beekeeper-helper] diskwait thread launched");

    DEBUG_LOG("[beekeeper-helper] Helper DBus service ready and waiting for calls");
    return app.exec();
}
