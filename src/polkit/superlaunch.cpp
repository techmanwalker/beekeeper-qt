// superlaunch.cpp

#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "globals.hpp"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

using beekeeper::privileged::supercommander;

namespace {
void registerMetaTypes()
{
    static bool registered = false;
    if (!registered) {
        // no-op

        registered = true;
    }
}
}
// -------------------- public API --------------------

bool
superlaunch::start_root_shell()
{
    std::lock_guard<std::mutex> lock(mtx_);
    return start_root_shell_unlocked();
}

supercommander&
superlaunch::create_commander()
{
    return *komander;
}

// -------------------- unlocked implementations --------------------

bool
superlaunch::start_root_shell_unlocked()
{
    supercommander &cmd = *komander;
    if (launcher->root_alive.load()) {
        launcher->already_set_root_alive_status.store(true);
        return true; // Already running
    }

    const QString service_name = QStringLiteral("org.beekeeper.Helper");
    QDBusConnection conn = QDBusConnection::systemBus();

    if (!conn.isConnected()) {
        qWarning() << "System bus not connected:" << conn.lastError().message();
        launcher->root_alive.store(false);
        launcher->already_set_root_alive_status.store(true);
        return false;
    }

    QDBusConnectionInterface *conn_iface = conn.interface();
    if (!conn_iface) {
        qWarning() << "Cannot obtain system bus interface";
        launcher->root_alive.store(false);
        launcher->already_set_root_alive_status.store(true);
        return false;
    }

    DEBUG_LOG("Attempting DBus activation for helper via StartServiceByName...");

    QDBusReply<void> r = conn_iface->startService(service_name);
    if (!r.isValid()) {
        qWarning() << "StartService request failed:" << r.error().message();
        launcher->root_alive.store(false);
        launcher->already_set_root_alive_status.store(true);
    } else {
        DEBUG_LOG("StartService request succeeded for:" , service_name);
    }

    // Wait for the service to actually appear
    QElapsedTimer timer;
    timer.start();
    const int timeout_ms = 10000;
    while (timer.elapsed() < timeout_ms && !conn_iface->isServiceRegistered(service_name)) {
        QThread::msleep(100);
    }

    if (!conn_iface->isServiceRegistered(service_name)) {
        // Fallback: ask systemd directly
        QDBusInterface systemd_iface(
            QStringLiteral("org.freedesktop.systemd1"),
            QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"),
            conn
        );
        if (systemd_iface.isValid()) {
            DEBUG_LOG("Attempting systemd StartUnit fallback for helper unit...");
            QDBusReply<QDBusObjectPath> sreply =
                systemd_iface.call(QStringLiteral("StartUnit"),
                                   QStringLiteral("org.beekeeper.helper.service"),
                                   QStringLiteral("replace"));
            if (!sreply.isValid()) {
                qWarning() << "StartUnit failed:" << sreply.error().message();
                launcher->root_alive.store(false);
                launcher->already_set_root_alive_status.store(true);
                return false;
            }

            // Wait again for DBus name
            timer.restart();
            while (timer.elapsed() < timeout_ms && !conn_iface->isServiceRegistered(service_name)) {
                QThread::msleep(100);
            }
            if (!conn_iface->isServiceRegistered(service_name)) {
                qWarning() << "Helper DBus service did not appear after StartUnit:" << service_name;
                launcher->root_alive.store(false);
                launcher->already_set_root_alive_status.store(true);
                return false;
            }
        } else {
            qWarning() << "systemd D-Bus interface not available; cannot StartUnit fallback";
            launcher->root_alive.store(false);
            launcher->already_set_root_alive_status.store(true);
            return false;
        }
    } else { 
        DEBUG_LOG("Helper already running, skipping StartService/StartUnit");
        launcher->root_alive.store(true);
        launcher->already_set_root_alive_status.store(true);
        return true;
    }

    DEBUG_LOG("Helper service registered on DBus: ", service_name);

    // Test if it is alive
    bool ok = false;
    if (!root_thread) return false; // the root thread is created right at start, this should not happen
    QMetaObject::invokeMethod(
        root_thread,
        [&]() { ok = root_thread->ping_helper(); },
        Qt::BlockingQueuedConnection
    );


    launcher->root_alive.store(true);
    launcher->already_set_root_alive_status.store(true);
    return true;
}