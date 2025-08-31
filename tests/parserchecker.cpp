#include "../include/beekeeper/commandmachine.hpp"
#include <iostream>

namespace cm = commandmachine;

int
testparsing(const std::map<std::string, std::string>&, 
                const std::vector<std::string>&);

std::vector<cm::Command> command_registry = {
    {
        "test", 
        testparsing,
        {
            {"option", "", false},
            {"anothercommandoption", "c", false},
            {"yetanotheroption", "x", true},
            {"koption", "k", true},
            {"Voption", "V", true}
        },
        "theoption",
        "Test parser behavior",
        0,      // min_subjects
        -1,     // max_subjects
        true    // Explicitly set disable_option_recognition
    }
};

int
testparsing(const std::map<std::string, std::string>& options, 
                const std::vector<std::string>& subjects) 
{
    for (const auto& option : options) {
        std::cout << "Option name: " << option.first << std::endl;
        std::cout << "Option value: " << option.second << std::endl;
    }

    return 1;
}

int
main(int argc, char* argv[]) {
    auto parser = cm::CommandParser::create();
    return parser->parse(command_registry, argc, argv);
}