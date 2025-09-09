// main.cpp
#include "../polkit/globals.hpp"
#include "mainwindow.hpp"
#include "rootshellthread.hpp"

#include "beekeeper/debug.hpp"
#include "beekeeper/cmakedependentvariables/translationsdir.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include <QApplication>
#include <QLocale>
#include <QObject>
#include <QTranslator>
#include <csignal>

using namespace beekeeper::privileged;

int
main(int argc, char *argv[])
{
    // Force early initialization on main thread
    init_globals();

    signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    DEBUG_LOG("[main] QApplication started, argc:", argc);

    // Load correct localization file
    QString locale = QLocale::system().name(); // ej: "es_MX"
    QString qm_file = QString("beekeeper-%1.qm").arg(locale.left(2)); // solo "es"
    QTranslator translator;

    QString path = QStringLiteral(TRANSLATIONS_DIR);
    if (translator.load("beekeeper-" + QLocale::system().name().left(2), TRANSLATIONS_DIR)) {
        app.installTranslator(&translator);
        DEBUG_LOG("[main] Translator loaded:", qm_file);
    } else {
        DEBUG_LOG("[main] Translator not found for:", qm_file);
    }
        
    MainWindow w;
    DEBUG_LOG("[main] MainWindow created, showing...");
    w.show();

    root_shell_thread *rootThread = new root_shell_thread(*launcher);
    w.set_root_thread(rootThread);

    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     rootThread, &QThread::quit);
    QObject::connect(rootThread, &QThread::finished,
                     rootThread, &QObject::deleteLater);

    QObject::connect(rootThread, &QThread::started,
                     rootThread, &root_shell_thread::init_root_shell);

    DEBUG_LOG("[main] Starting root_shell_thread...");
    rootThread->start();

    int exitCode = app.exec();

    // --- Phase 1: user-perceived exit ---
    w.hide(); // make it look like the app closed
    QCoreApplication::processEvents();

    // --- Phase 2: stop privileged helper and threads ---
    if (rootThread->isRunning()) {
        DEBUG_LOG("[main] Asking rootThread to quit safely...");
        rootThread->requestInterruption(); // cooperative shutdown
        rootThread->quit();                  // exit event loop
        if (!rootThread->wait(5000)) {
            DEBUG_LOG("[main] rootThread didn't stop in time; forcing terminate");
            rootThread->terminate();
            rootThread->wait();
        }
    }

    // --- Phase 3: teardown globals safely ---
    shutdown_globals(); // optional
    DEBUG_LOG("[main] Globals cleaned up, exiting.");
    DEBUG_LOG("[main] QApplication exec returned, exit code:", exitCode);
    return exitCode;
}

