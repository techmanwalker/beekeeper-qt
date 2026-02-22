#include "../core/clauses/bk-clauses.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <map>
#include <string>
#include <vector>

int
main (int argc, char **argv)
{
    // We'll do this using CLI11 as it's one of the few that support verbs
    // Declare that this program's name is "beekeeperman"

    CLI::App app{"beekeeperman"};

    // We'll store the parsed entities here
    std::string verb;
    std::map<std::string, std::string> options;
    std::vector<std::string> subjects;

    // Register each of the verbs dynamically
    for (const auto &[verb_name, meta] : clauses_registry) {
        // Register the verb and associate with its handler
        // verb.handleBy(meta.handler);

        CLI::App *sub = app.add_subcommand(verb_name, meta.description);
        sub->callback([&, verb_name]() {
            verb = verb_name;
        });

        // Associate with its own command options
        for (const auto &option : meta.allowed_options) {
            /*
            register_option_as_valid (
                option.long_name, // guaranteed to always be present
                (!option.short_name.empty() ? option.short_name : // don't register its short name)
                option.description, // make it so this program's help show its description
                option.requires_value // bail out if value is not present
            )
            */

            std::string opt_spec = "--" + option.long_name;

            if (!option.short_name.empty())
                opt_spec += ",-" + option.short_name;

            if (option.requires_value) {
                sub->add_option(
                    opt_spec,
                    options[option.long_name]
                    // ,option.description
                )->required(false);
            } else {
                sub->add_flag(
                    opt_spec,
                    [&](std::size_t) {
                        options[option.long_name] = "true";
                    }
                    // ,option.description
                );
            }
        }

        // Positional arguments that are not options are subjects
        sub->add_option(
            "subjects",
            subjects,
            "Subjects for this clause"
        );
    }

    // Now that all options are registered:

    // Parse the argc and argv so I have three entities:

    // one for the verb which is absolutely required to be right after the program's name, called std::string verb
    // one for associative options, preferably called std::map<std::string, std::string> options
    // one for alone values that don't belong to an option key, called std::vector<std::string> subjects

    try {
        app.require_subcommand(1);
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    // Now that I have the three entities, take the verb's associated clause, whose name is guaranteed to exist
    // in the clauses unordered_map due to the way the registration works.

    // The clause does all the handling job. beekeeperman is essentially a wrapper now.

    // Hence, the call trivially becomes:

    command_streams execution_result =
        clauses_registry.at(verb).handler(options, subjects);

    // And spit it out to the terminal, in this order: stderr, stdout and finally return with exit code.

    // This becomes:

    if (!execution_result.stderr_str.empty())
        std::cerr << execution_result.stderr_str << std::endl;

    if (!execution_result.stdout_str.empty())
        std::cout << execution_result.stdout_str << std::endl;

    return execution_result.errcode;
}
