#include "rootshellthread.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/util.hpp"
#include "beekeeper/supercommander.hpp"
#include "../polkit/globals.hpp"
#include <QDBusArgument>
#include <QDBusReply>

// A small helper QThread class to run the root shell

void
root_shell_thread::init_root_shell()
{
    DEBUG_LOG("[root_shell_thread] Attempting to start root shell...");
    if (!launcher_.start_root_shell()) {
        qCritical("Failed to start root shell in helper thread!");
        return;
    }
    DEBUG_LOG("[root_shell_thread] Root shell started successfully.");

    emit root_shell_ready();
}

bool
root_shell_thread::ping_helper()
{
    if (!ensure_iface())
        return false;

    QDBusReply<QVariantMap> r = the_iface->call("whoami");

    if (!r.isValid())
    {
        qWarning() << "DBus ping failed:" << r.error().message();
        return false;
    }

    QVariantMap m = r.value();

    if (!m.value("stderr").toString().isEmpty())
    {
        qWarning() << "Polkit denied:" << m.value("stderr").toString();
        return false;
    }

    return true;
}

bool
root_shell_thread::ensure_iface()
{
    if (!launcher->root_alive) {
        DEBUG_LOG("[supercommander] helper not alive");
        return false;
    }

    if (the_iface) {
        if (the_iface->thread() != QThread::currentThread()) {
            DEBUG_LOG("[supercommander] DBus iface in wrong thread → nuking");
            invalidate_iface();
        } else if (!the_iface->isValid()) {
            DEBUG_LOG("[supercommander] DBus iface invalid → nuking");
            invalidate_iface();
        }
    }

    if (!the_iface) {
        DEBUG_LOG("[supercommander] creating DBus interface");

        the_iface.reset(new QDBusInterface(
            "org.beekeeper.Helper",
            "/org/beekeeper/Helper",
            "org.beekeeper.Helper",
            QDBusConnection::systemBus()
        ));

        the_iface->setTimeout(120000);

        if (!the_iface->isValid()) {
            DEBUG_LOG("[supercommander] DBus iface creation failed:",
                      the_iface->lastError().message().toStdString());
            invalidate_iface();
            return false;
        }
    }

    return true;
}


bool
root_shell_thread::invalidate_iface()
{
    if (!the_iface)
        return false;

    QObject *obj = the_iface.get();

    if (obj->thread() == QThread::currentThread()) {
        delete obj;
    } else {
        QMetaObject::invokeMethod(obj, "deleteLater", Qt::QueuedConnection);
    }

    (void) the_iface.release(); // we no longer own it
    return true;
}

QFuture<command_streams>
root_shell_thread::call_bk_future(const QString &verb,
                                  const QVariantMap &options,
                                  const QStringList &subjects)
{
    auto promise = std::make_shared<QPromise<command_streams>>();
    auto future = promise->future();

    QMetaObject::invokeMethod(
        this,
        [this, promise, verb, options, subjects]()
        {
            if (!ensure_iface()) {
                command_streams r;
                r.stderr_str = "DBus interface not available";
                promise->addResult(r);
                promise->finish();
                return;
            }

            // ASYNCHRONIC DBus Call
            QDBusPendingCall call =
                the_iface->asyncCall("ExecuteCommand", verb, options, subjects);

            auto *watcher = new QDBusPendingCallWatcher(call, this);

            connect(watcher, &QDBusPendingCallWatcher::finished,
                    this,
                    [this, watcher, promise, verb]()
                    {
                        QScopedPointer<QDBusPendingCallWatcher, QScopedPointerDeleteLater> w(watcher);
                        command_streams result;

                        if (w->isError()) {
                            result.stderr_str = w->error().message().toStdString();
                        } else {
                            // <-- Aquí está la modificación principal
                            QDBusReply<QVariantMap> reply = *w;  

                            if (reply.isValid()) {
                                QVariantMap out_map = reply.value();

                                result.stdout_str = out_map.value("stdout").toString().toStdString();
                                result.stderr_str = out_map.value("stderr").toString().toStdString();
                            } else {
                                result.stderr_str = reply.error().message().toStdString();
                            }
                        }

                        DEBUG_LOG("call to ", verb, "returned:\n",
                                "   stdout: ", result.stdout_str, "\n",
                                "   stderr: ", result.stderr_str);

                        // Solve the future
                        promise->addResult(result);
                        promise->finish();

                        // Emit refresh signal
                        emit backend_command_finished(
                            verb,
                            QString::fromStdString(result.stdout_str),
                            QString::fromStdString(result.stderr_str)
                        );
                    });
        },
        Qt::QueuedConnection
    );

    return future;
}
