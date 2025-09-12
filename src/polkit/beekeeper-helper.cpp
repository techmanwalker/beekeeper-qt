// beekeeper-helper.cpp
#include "beekeeper-helper.hpp"
#include "../cli/handlers.hpp"
#include "beekeeper/internalaliases.hpp"
#include "../cli/commandregistry.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <sstream>

HelperObject::HelperObject(QObject *parent)
    : QObject(parent)
{
    run_autostart_tasks();
}

// Converts QVariantMap -> std::map<std::string, std::string>
static std::map<std::string, std::string>
convert_options(const QVariantMap &options)
{
    std::map<std::string, std::string> out;
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        out[it.key().toStdString()] = it.value().toString().toStdString();
    }
    return out;
}

// Converts QStringList -> std::vector<std::string>
static std::vector<std::string>
convert_subjects(const QStringList &subjects)
{
    std::vector<std::string> out;
    out.reserve(subjects.size());
    for (const QString &s : subjects) {
        out.push_back(s.toStdString());
    }
    return out;
}

void HelperObject::run_autostart_tasks()
{
    const char *flag_file = "/tmp/.beekeeper/already-ran";
    std::ifstream check_flag(flag_file);
    if (check_flag.good())
        return; // Already ran

    std::vector<std::string> uuids = bk_util::list_uuids_in_autostart();

    for (const std::string &uuid_str : uuids) {
        QString uuid = QString::fromStdString(uuid_str);
        QVariantMap empty_options;
        QStringList subject;
        subject << uuid;
        ExecuteCommand("start", empty_options, subject);
    }

    std::ofstream flag_out(flag_file);
    flag_out << "1\n";

    DEBUG_LOG("[beekeeper-helper] Autostart tasks completed");
}

QVariantMap
HelperObject::ExecuteCommand(const QString &verb,
                             const QVariantMap &options,
                             const QStringList &subjects)
{
    QVariantMap reply_map;
    reply_map.insert("stdout", QString());
    reply_map.insert("stderr", QString());

    std::string verb_std = verb.toStdString();
    std::map<std::string, std::string> opts_std = convert_options(options);
    std::vector<std::string> subs_std = convert_subjects(subjects);

    /*
    DEBUG_LOG("[beekeeper-helper] Received command verb: ", verb_std);
    DEBUG_LOG("[beekeeper-helper] Options size: ", std::to_string(opts_std.size()));
    DEBUG_LOG("[beekeeper-helper] Subjects size: ", std::to_string(subs_std.size()));
    */

    // Find handler in registry
    auto it = std::find_if(command_registry.begin(), command_registry.end(),
                           [&verb_std](const cm::command &cmd) {
                               return cmd.name == verb_std;
                           });

    if (it == command_registry.end()) {
        std::string err = "Unknown command verb: " + verb_std;
        DEBUG_LOG(err);
        reply_map["stderr"] = QString::fromStdString(err);
        return reply_map;
    }

    // Capture stdout/stderr
    std::stringstream cout_buf, cerr_buf;
    auto* old_cout = std::cout.rdbuf(cout_buf.rdbuf());
    auto* old_cerr = std::cerr.rdbuf(cerr_buf.rdbuf());

    int ret = it->handler(opts_std, subs_std);

    // Restore original streams
    std::cout.rdbuf(old_cout);
    std::cerr.rdbuf(old_cerr);

    // Fill reply_map with captured output
    reply_map["stdout"] = QString::fromStdString(cout_buf.str());
    reply_map["stderr"] = QString::fromStdString(cerr_buf.str());

    if (ret != 0) {
        QString err_text = reply_map["stderr"].toString();
        err_text += QString("Handler returned non-zero exit code: %1\n").arg(ret);
        reply_map["stderr"] = err_text;
    }

    return reply_map;
}

QVariantMap
HelperObject::whoami()
{
    QVariantMap reply_map;
    reply_map.insert("stdout", QString());
    reply_map.insert("stderr", QString());

    command_streams res = bk_util::exec_command("whoami");

    reply_map["stdout"] = QString::fromStdString(res.stdout_str);
    reply_map["stderr"] = QString::fromStdString(res.stderr_str);
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
        DEBUG_LOG("[beekeeper-helper] DBus name already owned, exiting cleanly");
        return 0;
    }

    if (!bus.registerService("org.beekeeper.Helper")) {
        std::cerr << "Failed to claim DBus name org.beekeeper.Helper\n";
        return 2;
    }

    DEBUG_LOG("[beekeeper-helper] registered service org.beekeeper.Helper");

    HelperObject helper;
    if (!bus.registerObject("/org/beekeeper/Helper", &helper,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        std::cerr << "Failed to register DBus object /org/beekeeper/Helper\n";
        return 1;
    }

    DEBUG_LOG("[beekeeper-helper] Helper DBus service ready and waiting for calls");
    return app.exec();
}
