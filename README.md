# beekeeper-qt

**Deduplicate redundant data in your disk and save space**    

**beekeeper-qt** lets you free up disk space by removing redundant block-level data, both inside files and across multiple files. This is done by **compression** and **deduplication** techniques which are discussed more in deep down below. This is a simple graphical interface to configure and run it without hassle.

> Transparent compression is now fully supported on beekeeper-qt on all the algorithms and levels the *btrfs* driver supports. Set it up along with deduplication with the *Setup* button.

![A quick screenshot I took to the UI. The CPU meter shows the total CPU usage of the entire system, not just by beekeeper-qt.](docs/ui.png)

*A quick screenshot I took to the UI. The CPU meter shows the total CPU usage of the entire system, not just by beekeeper-qt.*

**IMPORTANT:** beekeeper-qt works **exclusively** with the **btrfs** filesystem. It will **not** work with any other filesystem because the `bees` service by Zygo is only designed to work on *btrfs*.

## Why

Many disks, even those of everyday users, have a lot of repeated data taking up space unnecessarily:  

- Duplicate documents, photos, videos, or files repeated across different folders.  
- Programs and executables that contain multiple repeated binary overhead patterns.  
- Wine prefixes or virtual environments that replicate large chunks of data.  
- Database systems or servers storing repeated blocks.  

beekeeper-qt finds these redundant patterns and compresses or deduplicates them, freeing up space. Now that it supports **automatic transparent compression**, every new file written in your disk is compressed just-in-time to be able to fit more of your files in your disk, especially if it has a small capacity.

You can control the compression level with the **Setup** button and turn it off if you prefer, but note, **deduplication alone doesn't free up too much space**, hence why filesystem compression management was added.

When you open the program, it automatically obtains the privileges needed to deduplicate your disk via a system service, so you **don’t** need to type your password every time you want to use it.

## Features

- Deduplication of redundant data at the block level across your entire disk.
- Transparent file compression-to save even more space
- Quick configuration from the program **controls**.  
- Start deduplication automatically at system startup with one click.  
- CPU usage is minimal after the first deduplication.

## Installation

You can find packages for both `bees` and `beekeeper-qt` in this repo's [GitHub Releases](https://github.com/techmanwalker/beekeeper-qt/releases) page. They automatically pull the required dependencies.
> Note: for Ubuntu you need to [enable the *universe* repository](https://askubuntu.com/questions/148638/how-do-i-enable-the-universe-repository).

Root privileges are handled automatically by the systemd service.

> Note: `beekeeper-qt` is primarily written for Arch Linux and not thoroughly tested in other distros. Feel free to file an issue if bugs happen.

> Warning for Fedora and other SELinux-enabled distros: SELinux is not currently supported. We are currently looking for a proper module to support it and make it easy to use. For now, `beekeeper-qt` must be run in permissive mode. Read more at [security notes](#security-notes).

## Build - only do this if packages aren't available for your distro

You need Qt6 6.5+ to build beekeeper-qt. You'll also need some extra build dependencies for optimal runtime speed. I provide the full lists below.

To generate the installable packages for Arch Linux.

### Build for distros supported with CPack

Currently, only Fedora and Debian (.rpm and .deb) packages can be built with CPack.

If you are on Fedora, install dependencies with:

```
sudo dnf -y update
sudo dnf -y install \
    qt6-qtbase-devel qt6-qttools-devel \
    polkit-qt6-1-devel \
    libblkid-devel systemd-devel \
    gcc-c++ cmake ninja-build pkgconfig \
    rpm-build rpmdevtools make git m4 \
    selinux-policy-devel
```

On Debian:

```
sudo apt-get update
sudo apt-get install -y \
    qt6-base-dev qt6-tools-dev libpolkit-qt6-1-dev \
    libblkid-dev libudev-dev build-essential cmake ninja-build fakeroot dpkg-dev git pkgconf
```

On Arch:
```
sudo pacman -Syu --noconfirm
sudo pacman -S --noconfirm base-devel git cmake ninja \
    qt6-base qt6-tools polkit-qt6 systemd btrfs-progs \
    util-linux doxygen bees
```


To build `beekeeper-qt`:

```bash
git clone https://github.com/techmanwalker/beekeeper-qt
cd beekeeper-qt
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To generate `.deb` and `.rpm` packages:

```
cd build
cpack -G DEB
cpack -G RPM
```

CPack will print the package paths so you can copy them anywhere else.

If you instead want to quickly try out the compiled build, install it with:

```
sudo cmake --install build
```

And if you later want to switch to an official package, uninstall the build with:

```
sudo cmake --build build --target uninstall
```

Runtime dependencies are pulled by the packages when installed.

### Build for distros not supported by CPack

#### Build for Arch

To build for Arch, simply download and use the provided **PKGBUILD**.

1. Download the `PKGBUILD` under `packaging/` from this repository.
2. `cd` into the directory containing the `PKGBUILD`.
3. To build, run:

   ```bash
   makepkg -s
   ```
4. To install, run:

   ```bash
   makepkg -i
   ```

#### Build for Gentoo

For Gentoo, there's also an `.ebuild` file available under `packaging/`.

1. Download the `.ebuild` from this repository and the 
2. `cd` into the directory containing the `.ebuild` file.
3. Choose the version you want to build:

   * **Latest tagged version:**

     ```bash
     version="$(git describe --tags --abbrev=0 | sed 's/^v//')"
     ```
   * **Latest commit (live ebuild):**

     ```bash
     version=9999
     ```
4. Generate the final ebuild file:

   ```bash
   ebuildpath="beekeeper-qt-$version.ebuild"
   cp beekeeper-qt-9999.ebuild "$ebuildpath"
   ```
5. To prepare the manifest, run:

   ```bash
   ebuild "$ebuildpath" manifest
   ```
6. To build and install, run:

   ```bash
   ebuild "$ebuildpath" install
   ```

## Usage

1. Open beekeeper-qt and press **Setup** in the program controls.
2. Press Enter to accept default values or adjust them as needed.
3. Use the **+** button to enable deduplication at system startup (you can remove it later with **-**)

**Note:** The first time you run deduplication, CPU usage may reach near 100% depending on disk usage and hardware. This is normal and **only happens the first time; subsequent sessions will have minimal impact on CPU usage**. It’s recommended to do the first deduplication when you're not actively using the computer.

## Notes

* The first deduplication may take a while and consume a lot of CPU; following runs are much lighter.
* Transparent compression is now fully implemented.
* Works well both on regular desktops and on low-storage systems, and also useful for servers with repeated data patterns.

**FAQ / Quick tips**
- First deduplication may take some time; CPU usage can spike temporarily.
- Compression only applies to new files; run the one-time command for existing data (shown in Setup window).

## Security notes

`beekeeper-qt` has custom helper and D-Bus components. SELinux enforcement currently prevents the helper from functioning correctly. For now, you must run in permissive mode:

```
sudo setenforce 0
```

Once permissive, the helper will operate normally. You can re-enable enforcing afterward, but the helper will fail under strict policy due to the strictness of SELinux and you will see no filesystems listed and the main window will take really long to spawn because it can't start the root helper. 

*We are actively looking into a proper SELinux policy module; contributions or guidance are welcome.*

**UPDATE**: A partially written selinux policy is available under `src/polkit/burocracy`, but it is not functional because DBUs denies the messages with a `USER_AVC`. Any help is really appreciated.


## Contributions, License & Credits

* [https://github.com/necrose99](necrose99) for the `.ebuild` file!

* Pull requests are welcome. Please follow the current coding style and describe your changes clearly. Documentation is in [`docs/`](docs/).
* Licensed under [GPLv3](https://www.gnu.org/licenses/gpl-3.0.html)
* Special thanks to Zygo, for helping bring this project to life by creating [bees](https://github.com/Zygo/bees)!
* beekeeper-qt by itself is original work by [techmanwalker](https://github.com/techmanwalker).