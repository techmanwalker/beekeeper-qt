#pragma once

#include <QApplication>
#include <QFuture>
#include <QObject>
#include <QString>
#include <QThread>

#include "beekeeper/superlaunch.hpp"
#include "beekeeper/util.hpp"

// A small helper QThread class to run the root shell
class root_shell_thread : public QThread {
    Q_OBJECT
public:
    root_shell_thread(superlaunch &launcher, QObject *parent = nullptr)
        : QThread(parent), launcher_(launcher) {}

    void run() override
    {
        // Inicializamos la shell root
        init_root_shell();

        // Event loop necesario para que QDBusPendingCallWatcher funcione
        exec();
    }

public slots:
    
    QFuture<command_streams>
    call_bk_future(const QString &verb,
                                  const QVariantMap &options,
                                  const QStringList &subjects);
    bool ensure_iface ();
    bool invalidate_iface();
    bool ping_helper();
    void init_root_shell();
    
signals:
    void root_shell_ready();
    void backend_command_finished(const QString &cmd,
                          const QString &stdout_str,
                          const QString &stderr_str);

private:
    superlaunch &launcher_;

    std::unique_ptr<QDBusInterface> the_iface;
};