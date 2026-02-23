# The internal Clause protocol

Since v1.3.3, the process inter and internal communication is now fully unified as what's internally called the **clauses** protocol.

The clauses architecture consists of a key-paired **registry** where the keys are **verbs** (which can be thought of as _actions_) and the values are clause **entites** (an object describing the clause capabilities, behavior and limits). It can be more accurately described as "a declarative _command registry_ over imperative execution handlers" rather than a fully declarative architecture.

Among the roles and advantages of the clauses architecture, this project has demonstrated that they provide:

- Command-line interfaces derived directly from the registry definition
- Full and enforced feature parity between a program's GUI, CLI and optional daemon (if needed for system manipulation).
- Avoid overduplication of code across a project's binaries. A single project may use the same clause _registry_ to make a CLI and to allow easy IIPC and reduce boilerplate to replicate all roles at once.

Some potential advantages for general purpose programs:

- Auto-generated CLI derived from the registry
- Auto-generated Qt forms
- Full feature parity
- JSON RPC bridge
- Scripting interface
- Automatic testing harness
- Remote controlling

## Their role in beekeeper-qt

The clauses architecture introduces a declarative command registry layer that separates command metadata and constraints from the imperative execution logic. A clause is declared to have a handler, options list, description, limits and visibility attributes.

In **beekeeper-qt**, clauses allow the GUI to send and receive information to/from the daemon (**thebeekeeper**) for safe privilege escalation. The local `bk-clauses` registry enables code reutilization among all the GUI, CLI and daemon, providing and guaranteeing feature parity among all of the components of the program.

This is important so the user has the freedom of choice to use whatever interface they like to control their system. All three components are fully decoupled but they can communicate with each other via the `bk-clauses` registry definitions: the registry acts as a shared contract between components.

Ensuring full feature parity and easy inter-intra-process communication are fundamental features for this project's existence and proper behavior.




# Formal documentation

## Clause (entity)

A _clause_ is defined as an action or sentence, similar to a function prepared for inter-process communication, following a DBus-inspired fashion. A clause consists on:

- **clause handler**: a lambda function that receives a `clause_options` and a `clause_subjects`. This is the point where you define the behavior of the clause: what it actually does. Parsing and semantic validation of options and subjects is intentionally left to the handler implementation.

    In a well-designed program, a clause handler typically fulfills the following responsibilities:

    - validating subject count
    - normalizing short/long option names before invoking the handler.
    - handling each one of their subjects
    - processing, preparing and returning their defined data type properly

    A _handler_ can return any type of data depending on the developer and the project's needs, but the clause caller needs to handle the _clause handler_ return type, hence the caller needs to verify the handler return type in advance.


- **allowed options**: defines what the clause recognizes as a _valid_ modifier. Place the option **keys** your clause deems as valid here. It's up to the developer to reject or ignore invalid options and values for them inside the clause handler's logic.
- **subject name**: what a _subject_ is for your clause. Designed to break the generic name, you describe what kind of item or object your clause receives.

- **description**: describes your clause purpose or functionality.

- **min_subjects** / **max_subjects**: minimum and maximum count of subjects allowed. A value of `-1` in the latter means your clause accepts unlimited subjects.

- **hidden**: if true, do not show on the command line interface help guide for the end user (if such command line interface exists).

A clause doesn't hold its own name in its specification. It's designed to be paired with a verb string to ease development of a **clause registry** for easy extensibility and readability, in this format:

```c++
std::unordered_map<std::string, clause> clauses_registry = {
    {
        "verb",
        clause...
    },
    {
        "verb",
        clause...
    },
    {
        "verb",
        clause...
    }
}
```


You can read the [beekeeperman](../src/cli/beekeeperman.cpp) and [beekeeper-qt's clauses](../src/cli/clauses.cpp) code as a referennce on how you can use the clauses architecture to make a simple command line app by using _dynamic registration_.


## Clause call

A _clause call_ is a sentence, an action, which consists on

- **verb**: the name of the action
- **options**: A list of key:value pairs that alters the behavior of the clause as a set of rules
- **subjects**: A list of _subjects_\* the clause will operate based on its default behavior and its option modifiers.

_Note: every clause has its own definition on what the subjects actually **are**. In this project, it is often an UUID, that points to a filesystem._

### Example: wrapping an `ls` shell command as a clause

Suppose we have a clause registry with a simple clause that lists the directories of your current working folder:

```c++
std::vector<std::string>
list_clause_handler (const clause_options &options,
                     const clause_subjects &subjects)
{
    // ----- Options handling for this example -----

    // build the options string to pass to the shell
    std::string options_shell;

    /* watch for the options this clause supports
    * in this case, only "long-format" is supported
    */
    bool long_list_found = options.find("long-format") != options.end();

    // if the desired option was found, act accordingly
    if (long_list_found) {
        options_shell += " -l ";
    }

    // ----- End of options handling -----

    // ----- Subjects handling for this example -----

    /* This will often be easier and less boilerplate than 
    * option handling; often it consists in a single for
    * loop appending the subjects to an action chain.
    */

    // for this clause, a subject is a "file"
    // convert to shell format by sticking them together
    std::string files_shell;
    for (auto &subject : subjects) {
        files_shell += subject + " ";
    }

    return
        // separate output by spaces
        bk_util::tokenize(
            // call ls
            bk_util::exec_command("ls",
                options_shell, // append the already processed options
                " -- ", // end of command options mark
                files_shell // target directories; . by default
            )
        );
}

std::unordered_map<std::string, clause> registry = {
    "list",
    {
        list_clause_handler, // take the previously defined handler

        // recognized options
        {
            {
                "l", // short name
                "long-format", // long name
                false // it's not mandatory
            }
        },

        "file", // to show in the help guide what this clause handles

        "List files on given directory.", // a short description

        1, -1 // min. 1, max. unlimited subjects

        // hidden is false by default
    }
}
```

To perform a call to the just-defined clause, you:

- find its `key` in your clauses registry
- execute its handler
- pass the `clause_options` and `clause_subjects` as arguments
- optional: read the return value

For a simple C++ program that needs to list directories by using the previous clause:

```c++
#include "myclausesregistry.hpp"
#include <iostream>
#include <string>
#include <vector>

int
main ()
{
    // set options to call the clause
    // here we only set "long-format" to 1
    clause_options opts;
    opts["long-format"] = "1";

    // call your clause and store its output
    // "auto" is the return type of your clause handler
    auto output =
        // supposing you named your clauses registry as follows
        clauses_registry.at("list")

            // access to its handler
            .handler (
                // directly pass your previously defined options map
                opts,

                // and pass an empty subject list
                {}
            );

    /* To print the output, you just process whatever you defined
    * as your handler's return type.
    * In this case, it is an std::vector<std::string> as defined 
    * in our dummy list clause, so we'll  print it accordingly
    * and iterate over the listed directories.
    */
    std::cout << "Found files: \n\n";
    for (auto &file : output) {
        std::cout << file << '\n';
    }

    // flush the buffer right after couting for performance boost
    std::cout << std::endl;
}
```

And now you've coded your first clause in the `beekeeper-qt` style.

This may be added as a compilable example in the future.

---

## Appendix

### Default clause type definitions

_(if you'd like to use this architecture for your own project and adjust it to its own needs)..._

Clause calls parameters:

```c++
using clause_options  = std::map<std::string, std::string>;
using clause_subjects = std::vector<std::string>;
```

Clause handler:

```c++
using clause_handler = std::function<return_type(const clause_options&, 
                                                 const clause_subjects&)>;
```

Single option specification:

```c++
struct option_spec {
    std::string long_name;   // e.g. "enable-logging"
    std::string short_name;  // e.g. "l" (empty if no short form)
    bool requires_value;     // Does this option require a value?
};
```

Clause entity structure

```c++
struct clause {
    clause_handler handler; // clause behaviour
    std::vector<option_spec> allowed_options; // enables ignore/reject control
    std::string subject_name; // each subject item type
    std::string description; // short descriptions
    int min_subjects = 1;
    int max_subjects = -1; // -1: unlimited subjects
    bool hidden = false; // is it intended to be show to end user?
};
```

### FAQ

1. Why not DBus directly?

- This is a simplified version to make inter-and-intra-process communication (IIPC) easier to code. DBus can sometimes be messy to code and unexpected issues may arise.

And while they can be used for DBus communication, as we do use them in `beekeeper-qt`, it can be used in a number of roles other than just IIPC as stated at the beginning of this documentation page.