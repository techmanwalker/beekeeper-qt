# beekeeper: Btrfs File Deduplication Made Simple

[![License: AGPL3](https://img.shields.io/badge/License-AGPL-verysmall-green.svg)(https://www.gnu.org/licenses/agpl-3.0.txt)
[![Build Status](https://img.shields.io/github/actions/workflow/status/yourusername/beekeeper/build.yml)](https://github.com/yourusername/beekeeper/actions)

**Set up btrfs deduplication in a single command!** beekeeper simplifies storage optimization by managing the powerful beesd deduplication daemon through an intuitive interface. Reduce storage usage by eliminating duplicate files without complex configuration.

<p align="center">
  <img src="https://user-images.githubusercontent.com/328123/134765123-abcdef01-gh45-ijkl-mnop-qrstuvwxyz12.png" alt="beekeeper interface" width="600">
</p>

## Why beekeeper?

- 👹 **One-command deduplication** - Configure and start deduplication in seconds
- &#12850; **Automatic management** - Handles daemon lifecycle and log rotation
- 🨉 **Safe and reliable** - Built on battle-tested beesd technology
- 🚅 **Dual interfaces** - Choose between CLI and GUI (under development)

## Built on Zygo/bees

beekeeper is powered by the official [bees](https://github.com/Zygo/bees) deduplication engine from the btrfs developers. We simplify the setup and management while leveraging its powerful deduplication capabilities.

## Getting Started

### Prerequisites
- Linux with btrfs filesystems
- beesd installed ([installation guide](https://github.com/Zygo/bees#building))
- C++17 compatible compiler

### Installation

```bash
git clone https://github.com/yourusername/beekeeper.git
cd beekeeper
mkdir build && cd build
cmake . -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

## CLI Usage: `beesdman`

Manage deduplication from your terminal with simple commands:

```bash
# List available btrfs filesystems
beesdman list

# Configure deduplication for a filesystem
beesdman setup 22b6302a-51c8-4c7b-b0f0-40b7baa5421c

# Start deduplication
beesdman start 22b6302a-51c8-4c7b-b0f0-40b7baa5421c

# Monitor progress
beesdman log --follow 22b6302a-51c8-4c7b-b0f0-40b7baa5421c

# Check status
beesdman status 22b6302a-51c8-4c7b-b0f0-40b7baa5421c

# Stop deduplication
beesdman stop 22b6302a-51c8-4c7b-b0f0-40b7baa5421c
```

## GUI Version (under development)

Our graphical interface will provide a simple, intuitive way to manage deduplication:
- Visual filesystem selection
- One-click deduplication enable/disable
- Basic storage savings overview
- Log viewer with filtering

## Why It Works

beekeeper uses filesystem UUID to reliably identify storage volumes through:
- Reboots and remounts
- Device changes
- System upgrades
- Configuration updates

## Support

For support, please [open an issue](https://github.com/yourusername/beekeeper/issues) or join our [community forum](https://example.com/community).

## License

Distributed under the AGPL3 License. See `LICENSE` file for full details.

---

**Reclaim your storage today with beekeeper - the simple, focused solution for btrfs deduplication!**
