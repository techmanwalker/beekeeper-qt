#pragma once

#include <QApplication>
#include <QObject>
#include <QString>
#include <QThread>
#include "../polkit/globals.hpp"
#include "beekeeper/superlaunch.hpp"

// A small helper QThread class to run the root shell
class root_shell_thread : public QThread {
    Q_OBJECT
public:
    root_shell_thread(superlaunch &launcher, QObject *parent = nullptr)
        : QThread(parent), launcher_(launcher) {}

    void run() override;

public slots:
    void execute_command(const QString &cmd);
    
signals:
    void root_shell_ready();
    void command_finished(const QString &cmd,
                          const QString &stdout_str,
                          const QString &stderr_str);

private:
    superlaunch &launcher_;
};