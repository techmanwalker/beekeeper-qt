#include "beekeeper/beesdmgmt.hpp"
#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/commandmachine.hpp"
#include "beekeeper/internalaliases.hpp" // required for bk_mgmt and bk_util aliases
#include "beekeeper/util.hpp"
#include "commandregistry.hpp"
#include "handlers.hpp"
#include <iostream>

int
main(int argc, char* argv[]) {
    // beesd existence check
    if (!bk_util::command_exists("beesd")) {
        std::cerr << "Error: beesd is not installed\n";
        return 1;
    }

    // Create command parser
    auto parser = cm::command_parser::create();
    
    // Parse and execute command
    return parser->parse(command_registry, argc, argv);
}