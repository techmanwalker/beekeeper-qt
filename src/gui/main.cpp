#include "../polkit/globals.hpp"
#include "mainwindow.hpp"
#include "rootshellthread.hpp"

#include "../polkit/globals.hpp"

#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "translationsdir.hpp"
#include "beekeeper/util.hpp"
#include <QApplication>
#include <QLocale>
#include <QMessageBox>
#include <QObject>
#include <QString>
#include <QTranslator>
#include <csignal>

using namespace beekeeper::privileged;

int
main(int argc, char *argv[])
{
    bk_util::add_usr_sbin_to_path();
    
    // Force early initialization on main thread
    init_globals();

    signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    DEBUG_LOG("[main] QApplication started, argc:", argc);

    // Load correct localization file
    QString baseDir = QStringLiteral(TRANSLATIONS_DIR);
    QString fullLocale = QLocale::system().name();     // ex. "es_MX"
    QString langOnly = fullLocale.left(2);             // ex: "es"

    QTranslator* translator = new QTranslator(&app);

    auto tryLoad = [&](const QString& localeDir) -> bool {
        QString path = baseDir + "/" + localeDir + "/LC_MESSAGES/";
        return translator->load("beekeeper-qt", path);
    };

    // Using es_MX code for explanation

    // Try full locale (es_MX)
    if (tryLoad(fullLocale)) {
        app.installTranslator(translator);
    }
    // Try language only (es)
    else if (tryLoad(langOnly)) {
        app.installTranslator(translator);
    }
    // Search for any directory beginning with es_ ...
    else {
        QDir dir(baseDir);
        QStringList candidates = dir.entryList(
            QStringList() << (langOnly + "_*"),
            QDir::Dirs | QDir::NoDotAndDotDot
        );

        for (const QString& candidate : candidates) {
            if (tryLoad(candidate)) {
                app.installTranslator(translator);
                break;
            }
        }
    }

    // --- Check if beesd exists ---
    if (bk_util::which("beesd").empty()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle(QObject::tr("Bees daemon not found"));
        
        QString text = QObject::tr("Bees is not installed in your system.\nInstall it from: ");
        QString link = "<a href=\"https://github.com/techmanwalker/beekeeper-qt/releases\">https://github.com/techmanwalker/beekeeper-qt/releases</a>";

        msgBox.setTextFormat(Qt::RichText);
        msgBox.setTextInteractionFlags(Qt::TextBrowserInteraction);
        msgBox.setText(text + link);

        msgBox.exec();
        DEBUG_LOG("[main] beesd not found, exiting program.");
        return 1; // Abort execution
    }

    // --- User has permissions, continue normally ---
    MainWindow w;
    DEBUG_LOG("[main] MainWindow created, showing...");
    w.show();

    DEBUG_LOG("[main] Starting root_shell_thread...");

    int exitCode = app.exec();

    // --- Phase 1: user-perceived exit ---
    w.hide();
    QCoreApplication::processEvents();

    // --- Phase 2: teardown globals safely ---
    shutdown_globals();
    DEBUG_LOG("[main] Globals cleaned up, exiting.");
    DEBUG_LOG("[main] QApplication exec returned, exit code:", exitCode);
    return exitCode;
}
