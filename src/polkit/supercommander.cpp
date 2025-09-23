#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/supercommander.hpp"
#include "beekeeper/superlaunch.hpp"
#include <beekeeper/util.hpp>
#include <QDateTime>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>
#include <unistd.h>
#include <sys/poll.h>

#include "globals.hpp"

namespace beekeeper { namespace privileged {

// Execute a command in the DBus helper
command_streams
supercommander::call_bk(const QString &verb,
                        const QVariantMap &options,
                        const QStringList &subjects)
{
    // *
    // For debugging
    #ifdef BEEKEEPER_DEBUG_LOGGING
    // Serialize options into "key=value" pairs
    QStringList opts_serialized;
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        opts_serialized << QString("%1=%2").arg(it.key(), it.value().toString());
    }

    // Serialize subjects as space-separated list
    QString subjects_serialized = subjects.join(" ");

    DEBUG_LOG("Executing command in DBus root helper");
    DEBUG_LOG("verb: ", verb.toStdString());
    DEBUG_LOG("options: ", opts_serialized.join(", ").toStdString());
    DEBUG_LOG("subjects: ", subjects_serialized.toStdString());
    #endif
    //*/

    command_streams result;

    if (!launcher->root_alive) {
        DEBUG_LOG("[supercommander] helper not alive");
        return result;
    }

    QDBusInterface helper_iface(
        "org.beekeeper.Helper",
        "/org/beekeeper/Helper",
        "org.beekeeper.Helper",
        QDBusConnection::systemBus()
    );

    if (!helper_iface.isValid()) {
        DEBUG_LOG("[supercommander] helper DBus interface invalid:", helper_iface.lastError().message());
        result.stderr_str = helper_iface.lastError().message().toStdString();
        return result;
    }

    // DEBUG_LOG("[supercommander] call_bk: before DBus call for verb ", verb.toStdString(), " :" , QDateTime::currentDateTime().toString().toStdString());

    QDBusMessage reply_msg = helper_iface.call(QDBus::Block, "ExecuteCommand", verb, options, subjects);
    if (reply_msg.type() == QDBusMessage::ErrorMessage) {
        DEBUG_LOG("[supercommander] DBus call returned error: ", reply_msg.errorMessage());
        result.stderr_str = reply_msg.errorMessage().toStdString();
        return result;
    }

    // DEBUG_LOG("[supercommander] call_bk: after DBus call for verb ", verb.toStdString(), " :" , QDateTime::currentDateTime().toString().toStdString());

    if (reply_msg.arguments().isEmpty()) {
        DEBUG_LOG("[supercommander] helper reply had no arguments");
        return result;
    }

    QVariant arg = reply_msg.arguments().first();
    QVariantMap out_map;
    if (arg.canConvert<QVariantMap>()) {
        // Directly a QVariantMap
        out_map = arg.toMap();
    } else if (arg.canConvert<QDBusArgument>()) {
        // Wrapped as QDBusArgument → need to demarshal
        QDBusArgument dbus_arg = arg.value<QDBusArgument>();
        dbus_arg >> out_map;
    } else {
        DEBUG_LOG("[supercommander] helper reply in unexpected format, type=", arg.typeName());
        return result;
    }

    result.stdout_str = out_map.value("stdout").toString().toStdString();
    result.stderr_str = out_map.value("stderr").toString().toStdString();

    /*
    DEBUG_LOG("[supercommander] helper finished; stdout len=", result.stdout_str.size(),
              " stderr len=", result.stderr_str.size());
    */

    return result;
}

// Fully asynchronous call to the DBus helper
QFuture<command_streams>
supercommander::call_bk_async(const QString &verb,
                              const QVariantMap &options,
                              const QStringList &subjects)
{
    // QtConcurrent::run will execute call_bk in a separate thread
    return QtConcurrent::run([this, verb, options, subjects]() -> command_streams {
        return this->call_bk(verb, options, subjects);
    });
}

bool
supercommander::do_i_have_root_permissions()
{
    return launcher->root_alive;
}


// ------------------ High-level beekeeperman wrappers ------------------

std::vector<std::map<std::string,std::string>>
supercommander::btrfsls()
{
    QVariantMap opts;
    opts.insert("json", "<default>");

    command_streams res = call_bk("list", opts, QStringList{});
    std::vector<std::map<std::string,std::string>> result;

    if (!res.stdout_str.empty()) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(res.stdout_str), &parseError);
        if (parseError.error != QJsonParseError::NoError) return result;
        if (!doc.isArray()) return result;

        QJsonArray fsArray = doc.array();
        for (const QJsonValue &val : fsArray) {
            if (!val.isObject()) continue;
            QJsonObject obj = val.toObject();
            std::map<std::string,std::string> entry;
            entry["uuid"]   = obj.value("uuid").toString("").toStdString();
            entry["label"]  = obj.value("label").toString("").toStdString();
            entry["status"] = obj.value("status").toString("unknown").trimmed().toStdString();
            result.push_back(std::move(entry));
        }
    }

    return result;
}

std::string
supercommander::beesstatus(const std::string &uuid)
{
    command_streams res = call_bk("status", QVariantMap{}, QStringList(QString::fromStdString(uuid)));

    // Only care about stdout, ignore stderr
    std::string out = res.stdout_str;

    // Find the last ':' and grab everything after it
    auto pos = out.rfind(':');
    if (pos != std::string::npos && pos + 1 < out.size()) {
        out = out.substr(pos + 1);
    }

    // Trim leading/trailing whitespace
    size_t first = out.find_first_not_of(" \t\n\r");
    size_t last  = out.find_last_not_of(" \t\n\r");
    if (first == std::string::npos) return "stopped"; // empty -> stopped
    return out.substr(first, last - first + 1);
}

bool
supercommander::beesstart(const std::string &uuid, bool enable_logging)
{
    QVariantMap opts;
    if (enable_logging) opts.insert("enable-logging", "<default>");

    call_bk("start", opts, QStringList{QString::fromStdString(uuid)});
    return true;
}

bool
supercommander::beesstop(const std::string &uuid)
{
    call_bk("stop", QVariantMap{}, QStringList{QString::fromStdString(uuid)});
    return true;
}

bool
supercommander::beesrestart(const std::string &uuid)
{
    call_bk("restart", QVariantMap{}, QStringList{QString::fromStdString(uuid)});
    return true;
}

std::string
supercommander::beeslog(const std::string &uuid)
{
    command_streams res = call_bk("log", QVariantMap{}, QStringList{QString::fromStdString(uuid)});
    return res.stdout_str;
}

bool
supercommander::beesclean(const std::string &uuid)
{
    call_bk("clean", QVariantMap{}, QStringList{QString::fromStdString(uuid)});
    return true;
}

std::string
supercommander::beessetup(const std::string &uuid,
                          size_t db_size,
                          bool return_success_bool_instead)
{
    QVariantMap opts;
    if (db_size) opts.insert("db_size", static_cast<qulonglong>(db_size));
    opts.insert("json", "<default>");

    command_streams res = call_bk("setup", opts, QStringList{QString::fromStdString(uuid)});

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(res.stdout_str), &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return return_success_bool_instead ? "0" : "Failed to parse JSON";
    }

    QJsonObject obj = doc.object();
    int success = obj.value("success").toInt();
    QString message = obj.value("message").toString();

    if (return_success_bool_instead) {
        return success ? "1" : "0";
    } else {
        return message.toStdString();
    }
}

std::string
supercommander::beeslocate(const std::string &uuid)
{
    // No options needed
    QVariantMap opts;

    // Call the "locate" command
    command_streams res = call_bk("locate", opts, QStringList{QString::fromStdString(uuid)});

    if (!res.stdout_str.empty())
        return res.stdout_str;

    return "";  // return empty string if not mounted / not found
}


bool
supercommander::beesremoveconfig(const std::string &uuid)
{
    QVariantMap opts;
    opts.insert("remove", "<default>");

    command_streams res = call_bk("setup", opts, QStringList{QString::fromStdString(uuid)});

    return !res.stdout_str.empty();
}

std::string
supercommander::btrfstat(const std::string &uuid,
                         const std::string &mode)
{
    QVariantMap opts;
    if (!mode.empty()) {
        opts.insert("storage", QString::fromStdString(mode));
    }
    opts.insert("json", "<default>");

    command_streams res = call_bk("stat", opts, QStringList{QString::fromStdString(uuid)});

    if (res.stdout_str.empty()) {
        return "";
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(res.stdout_str), &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return "";
    }

    QJsonObject obj = doc.object();
    int success = obj.value("success").toInt();
    QString config_path = obj.value("config_path").toString();

    if (success == 1 && !config_path.isEmpty()) {
        return config_path.toStdString();
    }
    return "";
}

bool
supercommander::add_uuid_to_autostart(const std::string &uuid)
{
    QVariantMap opts;
    opts.insert("add", "<default>");

    command_streams res = call_bk("autostartctl", opts, QStringList(QString::fromStdString(uuid)));
    return true;
}

bool
supercommander::remove_uuid_from_autostart(const std::string &uuid)
{
    QVariantMap opts;
    opts.insert("remove", "<default>");

    command_streams res = call_bk("autostartctl", opts, QStringList(QString::fromStdString(uuid)));
    return true;
}

bool
supercommander::add_uuid_to_transparentcompression(const std::string &uuid,
                                                   const std::string &compression_level)
{
    QVariantMap opts;
    opts.insert("add", "<default>");
    opts.insert("compression-level", QString::fromStdString(bk_util::to_lower(compression_level)));

    command_streams res =
        call_bk("compressctl", opts, QStringList(QString::fromStdString(uuid)));

    return true;
}


bool
supercommander::remove_uuid_from_transparentcompression(const std::string &uuid)
{
    QVariantMap opts;
    opts.insert("remove", "<default>");

    command_streams res =
        call_bk("compressctl", opts, QStringList(QString::fromStdString(uuid)));

    return true;
}

bool
supercommander::start_transparentcompression_for_uuid(const std::string &uuid)
{
    QVariantMap opts;
    opts.insert("start", "<default>");

    command_streams res =
        call_bk("compressctl", opts, QStringList(QString::fromStdString(uuid)));

    return true;
}

bool
supercommander::pause_transparentcompression_for_uuid(const std::string &uuid)
{
    QVariantMap opts;
    opts.insert("pause", "<default>");

    command_streams res =
        call_bk("compressctl", opts, QStringList(QString::fromStdString(uuid)));

    return true;
}


} // namespace privileged
} // namespace beekeeper
