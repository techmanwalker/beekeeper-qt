# beekeeper Development Guide

## Architecture Overview

beekeeper follows a modular architecture with clear separation between:
- **Core Logic**: Filesystem management and deduplication control
- **CLI Interface**: Command-line interface for system administrators
- **GUI Interface**: Graphical interface for desktop users (under development)

```
project-root/
∐ include/              # Public headers
∣  ∔ beesd/
‣   ∔ beesdmgmt.h   # Daemon management API
‣   ∔ btrfssteup.h  # Configuration API

‣   ∔ util.h        # Utilities
• ∐ src/
∓  ∔ cli/              # CLI implementation
•  ∔ ∔ beesdman.cpp
‣  ∓  core/             # Core functionality
‣  ∔ beesdmgmt.cpp
‣  ∓  b_trfssetup.cpp
‣  ∔ util.cpp
‣  ∓ gui/              # GUI implementation (WIP)
∢  tests/                # Test suite
•4  ∔ beesdmgmttest.cpp
•4 ∔ b_trfsls.cpp
•4 ∔ setuppipeline.cpp
```

## Core API Reference

### Filesystem Management (`btrfssteup.h`)

#### `btrfsls()`
Lists available btrfs filesystems.

```cpp
std::vector<std::map<std::string, std::string>> btrfsls();
```

**Returns**  
Vector of filesystem objects with keys:
- `uuid`: Filesystem identifier (always present)
- `label`: Human-readable name (UUID if no label)

**Usage**:

```cpp
auto filesystems = btrfsls();
for (const auto& fs : filesystems) {
    std::cout << "Found: " << fs.at("label") 
              << " (" << fs.at("uuid") << ")\n";
}
```

#### `btrfstat()`

Checks if configuration exists for UUID.

```cpp
std::string btrfstat(std::string uuid);
```

**Returns**:  
Config file path if exists, empty string otherwise

#### `btrfsetup()`
Creates or updates configuration.

```cpp
std::string btrfsetup(std::string uuid, size_t db_size = 0);
```

**Parameters**:
- `uuid`: Filesystem identifier
- `db_size`: Deduplication database size in bytes:
  - `0`: Use default (1GB) for new configs, preserve existing value
  - `>0`: Set/override database size

**Behavior**:
1. Creates `/etc/bees` directory if missing
2. Generates UUID-based config filename
3. Preserves existing keys when updating
4. Orders keys alphabetically in output

### Deduplication Control (`beesdmgmt.h`)

#### Daemon Management
```cpp
bool beesstart(const std::string& uuid);     // Start daemon
bool beesstop(const std::string& uuid);      // Stop daemon
bool beesrestart(const std::string& uuid);  // Restart daemon
std::string beesstatus(const std::string& uuid); // Check status
```

**UUID-Centric Design**:  
All functions operate on filesystem UUID which:
1. Persist across reboots and remounts
2. Match beesd's configuration system
3. Prevent misidentification of filesystems

#### Log Management
```cpp
void beeslog(const std::string& uuid, bool follow = false);
```

## Test Suite

### 1. Filesystem Listing Test (`btrfsls.cpp`)
Validates btrfs filesystem detection:

```bash
./btrfsls
# Output: List of filesystems with labels and UUIDs
```

### 2. Configuration Pipeline Test (`setuppipeline.cpp`)
Tests config creation workflow:

```bash
./setuppipeline <uuid>
# Output: Verification of setup process
```

### 3. Daemon Management Test (`beesdmgmttest.cpp`)
End-to-end daemon control test:

```bash
./beesdmgmttest start <uuid>   # Verify start sequence
./beesdmgmttest status <uuid>  # Confirm running state
./beesdmgmttest log <uuid>     # Check log output
./beesdmgmttest stop <uuid>    # Validate clean shutdown
```

## Build Instructions

```bash
# Configure project
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build --parallel

# Run tests
ctest --test-dir build

# Install system-wide
sudo cmake --install build
```

## Contribution Guidelines

1. **Branch Naming**: `feature/name` or `fix/name`
2. **Code Style**: 
   - Functions: `lowercase_with_underscores`
   - Types: `PascalCase`
   - Braces on new lines for functions
3. **Testing**: 
   - Add tests for new features
   - Verify existing tests pass
4. **Documentation**: Update relevant .md files
5. **Commits**: Conventional commits with descriptive messages

## Why Bees?

beekeeper leverages the [Zygo/bees](https://github.com/Zygo/bees) deduplication engine because:
- 🨶 **Official Solution**: Developed by btrfs experts
- 🨉 **Block-Level Deduplication**: Finds duplicate data blocks
- 💂 **Efficient Hashing**: Uses cryptographic hashes for accuracy
- � **Proven Results**: Used in production systems worldwide

## Philosophy

beekeeper adheres to the UNIX philosophy:
- **KISS** (Keep It Simple Stupid): One purpose - simplify beesd setup
- **FDOS** (Don't Repeat Yourself): Reusable core code
- **FLOSS** Fully open source under AGPL3

## Licensing

beekeeper is licensed under the [AGPL3 License](https://www.gnu.org/licenses/agpl-3.0.txt), ensuring:
- **User Freedom**: Anyone can use, modify, and distribute the software
- **Community Benefit**: Contributions must be shared back
- **Service Variant**: Network use triggers source disclosure

## Contributing

Join our [development community](https://example.com/contribute) to help improve beekeeper! We welcome patches that:
- Adhere to the KISS philosophy
- Maintain code simplicity
- Improve reliability and error handling
- Add tests for new features
