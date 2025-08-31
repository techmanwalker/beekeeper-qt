// main.cpp
#include "../polkit/globals.hpp"
#include "mainwindow.hpp"
#include "rootshellthread.hpp"

#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include <QApplication>
#include <QObject>
#include <csignal>

using namespace beekeeper::privileged;

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    DEBUG_LOG("[main] QApplication started, argc:", argc);

    // Show main window immediately
    MainWindow w;
    DEBUG_LOG("[main] MainWindow created, showing...");
    w.show();

    // Now prepare root shell asynchronously
    auto &launcher = superlaunch::instance();
    DEBUG_LOG("[main] Launcher instance created.");

    root_shell_thread *rootThread = new root_shell_thread(launcher);
    // Provide it to MainWindow after construction
    w.set_root_thread(rootThread);
    QObject::connect(rootThread, &root_shell_thread::root_shell_ready, &w, &MainWindow::on_root_shell_ready);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, rootThread, &QThread::quit);
    QObject::connect(rootThread, &QThread::finished, rootThread, &QObject::deleteLater);

    DEBUG_LOG("[main] Starting root_shell_thread...");
    rootThread->start();

    int exitCode = app.exec();
    DEBUG_LOG("[main] QApplication exec returned, exit code:", exitCode);
    return exitCode;
}
