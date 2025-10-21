# beekeeper-qt Project Architecture

Welcome, dear user or coder. This is my very first public coding project, which is a graphical frontend interface made in C++/Qt that helps Linux system users free up a lot of disk space through file deduplication at the block level, thanks to Zygo's [bees](https://github.com/Zygo/bees) awesome project which this project leverages. Its name is **beekeeper-qt** and this page explains the project's architecture in general.

## Overview

The project is oriented toward the Linux desktop in general, but I also noted in the README.md that it can be useful on servers that handle a large amount of data with repeating patterns, and on systems that have very little available space (like systems with less than 64 GB of storage) to improve their performance and reduce their I/O operations.

## Architecture Components

Currently, its structure is divided into **three main components**, each with its own specific purpose:

### **beekeeperman** - Command Line Interface
The command-line utility that provides an interface prepared for those who prefer not to use a graphical interface and instead use a command line. Previously, it was the bridge between privileged space (root) and the graphical interface so the latter could function, but now these are independent since the creation of the next component.

### **beekeeper-helper** - Privileged Daemon
This is the other side of the graphical interface - the side you don't see. It's an executable that is launched through a systemd DBus-type service, so yes, it communicates with the graphical interface through DBus calls that serve to obtain privileged data and execute actions that otherwise couldn't be done, since privileges are needed to interact with the filesystem structure at a low level. Being it a DBus service, it doesn't ask you for your password when you launch beekeeper-qt as systemd *trusts* in the executable.

### **beekeeper-qt** - Graphical Interface
This is the graphical interface itself, made in Qt as its name suggests (screenshot available in my repo) that's designed to be simple and only put what's needed at hand. This is what controls the daemon called **bees** (which is the one that does the actual deduplication and I don't include it as another part of my project because I didn't write it), which additionally includes the functionality to start file deduplication at system boot and will include in the future the functionality to control the transparent compression settings offered by the BTRFS filesystem to increase the space-freeing capabilities of beekeeper-qt.

## Code Organization

As additional information about the architecture, **the code from all three parts is unified in the handlers.cpp file**, which is always the bridge between the heart of operations (which doesn't exist as a separate executable but rather as the source code itself, which would be **beesdmgmt.cpp** and **btrfsetup.cpp**) and all the points where it's presented to the user, such as:

- The DBus helper and messenger (**beekeeper-helper**, which you interact with through:)
- The graphical interface (**beekeeper-qt**)  
- The terminal interface (**beekeeperman**, this one *does* need sudo to work)

The last are the two ways in which the program is presented to the user.

## Programming Philosophy

The project follows an **"inside-out" execution flow** (or top-to-bottom if you think of it as a call tree starting from the main() functions), without going back to previous points in the flow of the same thread that shouldn't be reused. It relies heavily on division, refactorization, and reuse to minimize logic errors and allow focus solely on functionality.

**From now on, we're going to dive more on the programming philosophy of the project.**

## Key Design Decisions

The architecture emphasizes:

- **Separation of responsibilities** - Each component has a clear, distinct purpose
- **Privilege separation** - GUI runs unprivileged, daemon handles root operations
- **Code reuse** - Core logic is shared across all interfaces
- **Asynchronous operations** - QFutures keep the GUI responsive and efficient
- **Deterministic execution flow** - Simple, direct order from start to finish

This design ensures the program maintains clean, maintainable code while providing multiple access methods for different user preferences and use cases.

## Error Handling Philosophy

One thing you’ll notice in **beekeeper-qt** is that we don’t wrap every little operation in try/catch blocks. We follow what I like to call **“best-effort execution”**, which basically means:

* **Do the job, but only complain when necessary.**
  If a file doesn’t exist or a minor operation fails, a safe default value (like an empty vector or `false`) is returned. That’s why functions like `bk_util::read_lines_from_file()` never throw — they simply return an empty vector if the file can’t be read. No exceptions, no cluttered logs (as long as you don't build with `-DCMAKE_BUILD_TYPE=Debug`. That is intended only for development purposes)

* **Explicit checks for critical operations.**
  Whenever something could break the program or require root privileges (writing to `/etc`, controlling a systemd service, etc.), the program itself does explicit checks first with helpers like `file_readable()`, `is_root()`, `is_uuid()`, or `quote_if_needed()`. Only then do we “complain” or refuse to continue.

* **Separation of concerns.**
  GUI operations, DBus calls, and high-level commands don’t get bogged down in low-level error handling. They rely on the core utilities to fail gracefully. These are my best effort to make the code predictable, easy to follow, and reduce unexpected crashes (file an issue if you find one)

* **Predictable defaults.**
  Users (or other parts of the program) always get a meaningful result, even when something goes wrong behind the scenes. For example:

  ```cpp
  std::vector<std::string> lines = bk_util::read_lines_from_file("/some/config");
  // lines might be empty if the file doesn't exist — program continues safely
  ```

This approach lets developers focus on **what the program does**, not every little failure that *might* happen along the way. It’s inspired by the philosophy of classic Linux system utilities: fail quietly when it’s harmless, but fail loudly when it matters.

> TL;DR: If a file is missing, we skip it. If a critical action can’t be performed safely, we stop and report it. Everything in between is “best-effort execution.”