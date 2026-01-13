#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/internalaliases.hpp"
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

// thread-local storage: each thread has its own interface


// Execute a command in the DBus helper

QFuture<command_streams>
supercommander::call_bk_future(const QString &verb,
                               const QVariantMap &options,
                               const QStringList &subjects)
{
    if (!root_thread)
    {
        QPromise<command_streams> p;
        p.start();
        command_streams r;
        r.stderr_str = "root_shell_thread not running";
        p.addResult(r);
        p.finish();
        return p.future();
    }

    return root_thread->call_bk_future(verb, options, subjects);
};

bool
supercommander::do_i_have_root_permissions()
{
    return launcher->root_alive;
}


// ------------------ High-level beekeeperman wrappers (async QFuture version) ------------------

QFuture<fs_map>
supercommander::btrfsls()
{
    QVariantMap opts;
    opts.insert("json", "<default>");

    return root_thread->call_bk_future("list", opts, QStringList{}).then([](command_streams res) {
        fs_map result;

        if (!res.stdout_str.empty()) {
            DEBUG_LOG("RECEIVED BY btrfsls(): ", res.stdout_str);
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(res.stdout_str), &parseError);
            if (parseError.error != QJsonParseError::NoError) return result;
            if (!doc.isArray()) return result;

            QJsonArray fsArray = doc.array();
            for (const QJsonValue &val : fsArray) {
                if (!val.isObject()) continue;
                QJsonObject obj = val.toObject();

                result.emplace(
                    obj.value("uuid").toString("").toStdString(),
                    fs_info {
                        obj.value("label").toString("").toStdString(),
                        obj.value("status").toString("unknown").trimmed().toStdString(),
                        obj.value("devname").toString("unknown").trimmed().toStdString(),
                        obj.value("config").toString("unknown").trimmed().toStdString(),
                        obj.value("compressing").toBool(false),
                        obj.value("autostart").toBool(false)
                    }
                );
            }
        }

        #ifdef BEEKEEPER_DEBUG_LOGGING
        DEBUG_LOG("Found filesystems: ");
        for (const auto &[uuid, info]: result) {
            DEBUG_LOG(
                "FS: uuid=", uuid, "\n",
                " label=", info.label, "\n",
                " status=", info.status, "\n",
                " devname=", info.devname, "\n",
                " config=", info.config, "\n",
                " compressing=", info.compressing, "\n",
                " autostart=", info.autostart, "\n"
            );
        }
        #endif

        return result;
    });
}

QFuture<std::string>
supercommander::beesstatus(const QString &uuid)
{
    return root_thread->call_bk_future("status", QVariantMap{}, QStringList{uuid})
        .then([](command_streams res) -> std::string {
            std::string out = res.stdout_str;

            auto pos = out.rfind(':');
            if (pos != std::string::npos && pos + 1 < out.size()) {
                out = out.substr(pos + 1);
            }

            size_t first = out.find_first_not_of(" \t\n\r");
            size_t last  = out.find_last_not_of(" \t\n\r");
            if (first == std::string::npos) return "stopped";
            return out.substr(first, last - first + 1);
        });
}

QFuture<bool>
supercommander::beesstart(const QString &uuid, bool enable_logging)
{
    QVariantMap opts;
    if (enable_logging)
        opts.insert("enable-logging", "<default>");

    return root_thread->call_bk_future("start", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::beesstop(const QString &uuid)
{
    return root_thread->call_bk_future("stop", QVariantMap{}, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::beesrestart(const QString &uuid)
{
    return root_thread->call_bk_future("restart", QVariantMap{}, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<std::string>
supercommander::beeslog(const QString &uuid)
{
    return root_thread->call_bk_future("log", QVariantMap{}, QStringList{uuid})
        .then([](command_streams res) { return res.stdout_str; });
}

QFuture<bool>
supercommander::beesclean(const QString &uuid)
{
    return root_thread->call_bk_future("clean", QVariantMap{}, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<std::string>
supercommander::beessetup(const QString &uuid,
                          size_t db_size,
                          bool return_success_bool_instead)
{
    QVariantMap opts;
    if (db_size) opts.insert("db_size", static_cast<qulonglong>(db_size));
    opts.insert("json", "<default>");

    return root_thread->call_bk_future("setup", opts, QStringList{uuid})
        .then([return_success_bool_instead](command_streams res) -> std::string {
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
        });
}

QFuture<std::string>
supercommander::beeslocate(const QString &uuid)
{
    return root_thread->call_bk_future("locate", QVariantMap{}, QStringList{uuid})
        .then([](command_streams res) {
            return res.stdout_str.empty() ? "" : res.stdout_str;
        });
}

QFuture<bool>
supercommander::beesremoveconfig(const QString &uuid)
{
    QVariantMap opts;
    opts.insert("remove", "<default>");

    return root_thread->call_bk_future("setup", opts, QStringList{uuid})
        .then([](command_streams) { return !command_streams{}.stdout_str.empty(); });
}

QFuture<std::string>
supercommander::btrfstat(const QString &uuid, const QString &mode)
{
    QVariantMap opts;
    if (!mode.isEmpty()) opts.insert("storage", mode);
    opts.insert("json", "<default>");

    return root_thread->call_bk_future("stat", opts, QStringList{uuid})
        .then([mode](command_streams res) -> std::string {
            if (res.stdout_str.empty()) return "";

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(
                QByteArray::fromStdString(res.stdout_str), &err);

            if (err.error != QJsonParseError::NoError || !doc.isObject()) return "";

            QJsonObject obj = doc.object();
            int success = obj.value("success").toInt();

            auto qjsonvalue_to_string = [](const QJsonValue &v) -> std::string {
                if (v.isString()) return v.toString().toStdString();
                if (v.isDouble()) return std::to_string(static_cast<qint64>(v.toDouble()));
                return "";
            };

            if (!mode.isEmpty()) {
                if (success != 1) return "";
                if (mode == "free") return qjsonvalue_to_string(obj.value("free"));
                if (mode == "used") return qjsonvalue_to_string(obj.value("used"));
                return "";
            }

            QString config_path = obj.value("config_path").toString();
            if (success == 1 && !config_path.isEmpty()) return config_path.toStdString();
            return "";
        });
}

QFuture<bool>
supercommander::add_uuid_to_autostart(const QString &uuid)
{
    QVariantMap opts;
    opts.insert("add", "<default>");

    return root_thread->call_bk_future("autostartctl", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::remove_uuid_from_autostart(const QString &uuid)
{
    QVariantMap opts;
    opts.insert("remove", "<default>");

    return root_thread->call_bk_future("autostartctl", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::add_uuid_to_transparentcompression(const QString &uuid,
                                                   const QString &compression_level)
{
    QVariantMap opts;
    opts.insert("add", "<default>");
    opts.insert("compression-level", QString::fromStdString(compression_level.toLower().toStdString()));

    return root_thread->call_bk_future("compressctl", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::remove_uuid_from_transparentcompression(const QString &uuid)
{
    QVariantMap opts;
    opts.insert("remove", "<default>");

    return root_thread->call_bk_future("compressctl", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::start_transparentcompression_for_uuid(const QString &uuid)
{
    QVariantMap opts;
    opts.insert("start", "<default>");

    return root_thread->call_bk_future("compressctl", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}

QFuture<bool>
supercommander::pause_transparentcompression_for_uuid(const QString &uuid)
{
    QVariantMap opts;
    opts.insert("pause", "<default>");

    return root_thread->call_bk_future("compressctl", opts, QStringList{uuid})
        .then([](command_streams) { return true; });
}



} // namespace privileged
} // namespace beekeeper
