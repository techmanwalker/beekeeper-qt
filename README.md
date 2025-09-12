# beekeeper-qt

**Deduplicate redundant data in your disk and save space**  

**IMPORTANT:** beekeeper-qt works **EXCLUSIVELY** with the BTRFS filesystem. It will **not** work with any other filesystem.  

beekeeper-qt lets you free up disk space by removing redundant block-level data, both inside files and across different files. It is based on [bees](https://github.com/Zygo/bees), which does the actual deduplication, and gives you a simple graphical interface to configure and run it without hassle.

> For now it only performs deduplication; transparent compression is planned for the future. In many cases, deduplication alone already covers what compression would handle.

![A quick screenshot I took to the UI. The CPU meter shows the total CPU usage of the entire system, not just by beekeeper-qt.](docs/ui.png)

*A quick screenshot I took to the UI. The CPU meter shows the total CPU usage of the entire system, not just by beekeeper-qt.*

## Problems it solves

Many disks, even those of everyday users, have a lot of repeated data taking up space unnecessarily:  

- Duplicate documents and files, like photos, videos, or files repeated across different folders.  
- Programs and executables that contain multiple repeated binary overhead patterns.  
- Wine prefixes or virtual environments that replicate large chunks of data.  
- Database systems or servers storing repeated blocks.  

With beekeeper-qt, you can find these redundant data patterns and safely remove them, freeing up space easily.  

When you open the program, it automatically obtains the privileges needed to deduplicate your disk via a system service, so you **don’t** need to type your password every time you want to use it.

## Features

- Deduplication of redundant data at the block level across your entire disk.  
- Quick configuration from the program **controls**.  
- Start deduplication automatically at system startup with one click.  
- CPU usage is minimal after the first deduplication.

## Installation

You need Qt6 and bees. On Arch Linux, install bees with:

```bash
sudo pacman -S bees
````

To build beekeeper-qt:

```bash
git clone <repo-url>
cd beekeeper-qt
cmake -B build -G Ninja
cmake --build build
```

No further dependencies are required. Root privileges are handled automatically by a system service.

## Usage

1. Open beekeeper-qt and press **Configure** in the program controls.
2. Press Enter to accept default values or adjust them as needed.
3. Use the **+** button to enable deduplication at system startup.
4. The first time you run deduplication, CPU usage may reach near 100% depending on disk usage and hardware. This is normal and only happens the first time; subsequent sessions will have minimal impact. It’s recommended to do the first deduplication when not actively using the computer.

## Notes

* The first deduplication may take a while and consume a lot of CPU; following runs are much lighter.
* Transparent compression is not yet implemented. Currently, only deduplication is available.
* Works well both on regular desktops and on low-storage systems, and also useful for servers with repeated data patterns.

## Contributions, License & Credits

* Pull requests are welcome. Please follow the current coding style and describe your changes clearly. Documentation is in [`docs/`](docs/).
* Licensed under [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.html).
* Special thanks to Zygo, for helping bring this project to life by creating [Zygo/bees](https://github.com/Zygo/bees).
