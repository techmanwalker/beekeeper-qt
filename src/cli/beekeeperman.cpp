#include "../core/clauses/bk-clauses.hpp"
#include "translationsdir.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QStringList>

int main(int argc, char **argv)
{
    // 1. Create QCoreApplication FIRST (removes Qt args from argc/argv)
    QCoreApplication qt_app(argc, argv);
    
    // 2. Load translator using same logic as GUI
    QString baseDir = QStringLiteral(TRANSLATIONS_DIR);
    QString fullLocale = QLocale::system().name();     // ex. "es_MX"
    QString langOnly = fullLocale.left(2);             // ex: "es"
    
    QTranslator translator;
    bool translatorLoaded = false;
    
    auto tryLoad = [&](const QString& localeDir) -> bool {
        QString path = baseDir + "/" + localeDir + "/LC_MESSAGES/";
        return translator.load("beekeeper-qt", path);
    };
    
    // Try full locale (es_MX)
    if (tryLoad(fullLocale)) {
        translatorLoaded = true;
    }
    // Try language only (es)
    else if (tryLoad(langOnly)) {
        translatorLoaded = true;
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
                translatorLoaded = true;
                break;
            }
        }
    }
    
    if (translatorLoaded) {
        qt_app.installTranslator(&translator);

        #ifdef BEEKEEPER_DEBUG_LOGGING
        std::cout << QObject::tr("If you see this string translated, translations are being applied to CLI!").toStdString() << std::endl;
        #endif
    }
    
    // 3. NOW safe to access registry (tr() will translate using installed translator)
    const auto& clauses_registry = clauses_registry::get();
    
    // 4. CLI11 setup (uses translated descriptions)
    CLI::App app{"beekeeperman"};
    std::string verb;
    std::map<std::string, std::string> options;
    std::vector<std::string> subjects;
    
    // Register each of the verbs dynamically
    for (const auto &[verb_name, meta] : clauses_registry) {
        // meta.description is now translated if a .qm file was found
        CLI::App *sub = app.add_subcommand(verb_name, meta.description);
        sub->callback([&, verb_name]() {
            verb = verb_name;
        });
        
        // ... rest of your option setup unchanged ...
        for (const auto &option : meta.allowed_options) {
            std::string opt_spec = "--" + option.long_name;
            if (!option.short_name.empty())
                opt_spec += ",-" + option.short_name;
            
            if (option.requires_value) {
                sub->add_option(opt_spec, options[option.long_name]);
            } else {
                sub->add_flag(opt_spec, [&](std::size_t) {
                    options[option.long_name] = "true";
                });
            }
        }
        
        sub->add_option("subjects", subjects, "Subjects for this clause");
    }
    
    // 5. Parse and execute
    try {
        app.require_subcommand(1);
        app.parse(argc, argv);  // argc/argv already cleaned by QCoreApplication
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }
    
    command_streams execution_result =
        clauses_registry.at(verb).handler(options, subjects);
    
    if (!execution_result.stderr_str.empty())
        std::cerr << execution_result.stderr_str << std::endl;
    
    if (!execution_result.stdout_str.empty())
        std::cout << execution_result.stdout_str << std::endl;
    
    return execution_result.errcode;
    // translator and qt_app destroyed here
}