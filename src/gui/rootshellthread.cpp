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
    DEBUG_LOG("Sending message to DBus: \n",
    "   verb: ", verb,
    "   options: ", options,
    "   subjects:", subjects);

    auto promise = std::make_shared<QPromise<command_streams>>();
    auto future  = promise->future();

    QMetaObject::invokeMethod(
        this,
        [this, promise, verb, options, subjects]()
        {
            if (!ensure_iface()) {
                command_streams r;
                r.errcode = 1;
                r.stderr_str = "DBus interface not available";
                promise->addResult(r);
                promise->finish();
                return;
            }

            // Async DBus call (helper uses delayed reply)
            QDBusPendingCall call =
                the_iface->asyncCall(
                    "execute_clause",
                    verb,
                    options,
                    subjects
                );

            auto *watcher = new QDBusPendingCallWatcher(call, this);

            connect(
                watcher,
                &QDBusPendingCallWatcher::finished,
                this,
                [promise, watcher, verb, options, subjects]()
                {
                    QScopedPointer<QDBusPendingCallWatcher,
                                   QScopedPointerDeleteLater> w(watcher);

                    command_streams result;

                    if (w->isError()) {
                        result.errcode = 1;
                        result.stderr_str =
                            w->error().message().toStdString();
                    } else {
                        QDBusReply<QVariantMap> reply = *w;

                        if (reply.isValid()) {
                            QVariantMap m = reply.value();

                            result.stdout_str =
                                m.value("stdout_str").toString().toStdString();
                            result.stderr_str =
                                m.value("stderr_str").toString().toStdString();
                            result.errcode =
                                m.value("errcode").toInt();
                        } else {
                            result.errcode = 1;
                            result.stderr_str =
                                reply.error().message().toStdString();
                        }
                    }

                    DEBUG_LOG("For message sent via DBus: \n",
                    ">>>"
                    "   verb: ", verb, "\n",
                    "   options: ", options, "\n",
                    "   subjects: ", subjects, "\n",
                    "===", "\n",
                    "   Received:", "\n",
                    "===", "\n",
                    "   stdout_str: ", result.stdout_str, "\n",
                    "   stderr_str: ", result.stderr_str, "\n",
                    );

                    promise->addResult(result);
                    promise->finish();
                }
            );
        },
        Qt::QueuedConnection
    );

    return future;
}
