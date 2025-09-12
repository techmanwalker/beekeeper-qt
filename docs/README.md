# beekeeper-qt Project Architecture

Welcome, dear user or coder. This is my very first public coding project, which is a graphical frontend interface made in C++/Qt that helps Linux system users free up a lot of disk space through file deduplication at the block level, thanks to Zygo's [bees](https://github.com/Zygo/bees) awesome project which this project leverages on. Its name is **beekeeper-qt** and this project explains the project's architecture in general.

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

The project follows a **"inside-out" execution flow** (or top-to-bottom if you think of it as a call tree starting from the main() functions), without going back to previous points in the flow of the same thread that shouldn't be reused. It relies heavily on division, refactorization, and reuse to minimize logic errors and allow focus solely on functionality.

## Key Design Decisions

The architecture emphasizes:

- **Separation of responsibilities** - Each component has a clear, distinct purpose
- **Privilege separation** - GUI runs unprivileged, daemon handles root operations
- **Code reuse** - Core logic is shared across all interfaces
- **Asynchronous operations** - QFutures keep the GUI responsive and efficient
- **Deterministic execution flow** - Simple, direct order from start to finish

This design ensures the program maintains clean, maintainable code while providing multiple access methods for different user preferences and use cases.
