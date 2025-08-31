#include "beekeeper/supercommander.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/debug.hpp"
#include <csignal>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>
#include <unistd.h>
#include <sys/poll.h>
#include <sstream>

#include "globals.hpp"
#include "socketoperations.hpp"
using namespace beekeeper::privileged::socketops;

namespace beekeeper { namespace privileged {

supercommander::~supercommander() {
    if (superlaunch::instance().root_shell_alive())
        superlaunch::instance().stop_root_shell();
}

command_streams
supercommander::execute_command_in_forked_shell(const std::string &subcmd) {
    signal(SIGPIPE, SIG_IGN);
    command_streams result;
    auto &helper = superlaunch::instance();
    if (!helper.root_shell_alive()) {
        DEBUG_LOG("[supercommander] helper not alive");
        return result;
    }

    int fd = root_stdin_fd_;

    DEBUG_LOG("[supercommander] about to write command len=", subcmd.size());

    if (!write_message(fd, subcmd)) {
        DEBUG_LOG("[supercommander] write_message failed; fd=", fd,
                  " errno=", errno, " strerr=", strerror(errno));
        return result;
    }
    DEBUG_LOG("[supercommander] write_message succeeded, waiting for reply...");

    std::string encoded = read_message(fd);
    if (encoded.empty()) {
        DEBUG_LOG("[supercommander] read_message returned empty (peer closed or error)");
        return result;
    }

    DEBUG_LOG("[supercommander] received encoded reply (len=", encoded.size(), "): ", encoded);

    // Decode JSON reply (helper sends { stdout: "...", stderr: "..." })
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(encoded), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        DEBUG_LOG("[supercommander] Failed to parse helper JSON response:", parseError.errorString().toStdString());
        return result;
    }

    QJsonObject obj = doc.object();
    result.stdout_str = obj.value("stdout").toString().toStdString();
    result.stderr_str = obj.value("stderr").toString().toStdString();

    return result;
}


void
supercommander::execute_command_in_forked_shell_async(const std::string &subcmd)
{
    (void)QtConcurrent::run([this, subcmd](){
        command_streams res = execute_command_in_forked_shell(subcmd);
        emit command_finished(QString::fromStdString(subcmd),
                              QString::fromStdString(res.stdout_str),
                              QString::fromStdString(res.stderr_str));
    });
}

bool
supercommander::do_i_have_root_permissions()
{
    return superlaunch::instance().root_shell_alive();
}


// ------------------ High-level beekeeperman wrappers ------------------

std::vector<std::map<std::string,std::string>>
supercommander::btrfsls()
{
    std::vector<std::map<std::string,std::string>> result;

    auto cmd_res = execute_command_in_forked_shell("/usr/local/bin/beekeeperman list -j");
    if (!cmd_res.stdout_str.empty()) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(cmd_res.stdout_str), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "Failed to parse JSON:" << parseError.errorString();
            return result;
        }

        if (!doc.isArray()) {
            qDebug() << "Expected JSON array from beekeeperman list";
            return result;
        }

        QJsonArray fsArray = doc.array();
        for (const QJsonValue &val : fsArray) {
            if (!val.isObject()) continue;
            QJsonObject obj = val.toObject();

            std::map<std::string,std::string> entry;
            entry["uuid"]   = obj.value("uuid").toString("").toStdString();
            entry["label"]  = obj.value("label").toString("").toStdString();
            entry["status"] = obj.value("status").toString("stopped").toStdString();
            result.push_back(std::move(entry));
        }
    }

    return result;
}

std::string
supercommander::beesstatus(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell("/usr/local/bin/beekeeperman status " + uuid);

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
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " start " + uuid);
    Q_UNUSED(res)
    return true; // simplification; could check stdout/stderr
}

bool
supercommander::beesstop(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " stop " + uuid);
    Q_UNUSED(res)
    return true;
}

bool
supercommander::beesrestart(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " restart " + uuid);
    Q_UNUSED(res)
    return true;
}

std::string
supercommander::beeslog(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " log " + uuid);
    return res.stdout_str;
}

bool
supercommander::beesclean(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " clean " + uuid);
    Q_UNUSED(res)
    return true;
}

std::string
supercommander::beessetup(const std::string &uuid, size_t db_size)
{
    std::string cmd = beekeepermanpath + " setup --db-size " + std::to_string(db_size) + " " + uuid;
    command_streams res = execute_command_in_forked_shell(cmd);
    if (!res.stdout_str.empty())
        return res.stdout_str;
    return "";
}

bool
supercommander::beesremoveconfig(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " remove-config " + uuid);
    Q_UNUSED(res)
    return true;
}

std::string
supercommander::btrfstat(const std::string &uuid)
{
    command_streams res = execute_command_in_forked_shell(beekeepermanpath + " stat " + uuid);
    return res.stdout_str;
}

} // namespace privileged
} // namespace beekeeper
