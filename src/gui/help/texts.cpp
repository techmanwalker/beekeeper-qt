#include "texts.hpp"

QString
helptexts::keyboardnav ()
{
    return tr("# Keyboard navigation in beekeeper-qt") + "\n\n"
        + tr("## 1. Table navigation") + "\n"
        + tr("- Move the highlight between filesystems using **↑** and **↓** arrows. This moves the visual hover without selecting.") + "\n"
        + tr("- Press **Enter** or **Space** to select the highlighted filesystem.") + "\n"
        + tr("- To select multiple rows:") + "\n"
        + "- **Ctrl + Enter** → " + tr("add only the currently highlighted row to the selection.") + "\n"
        + "- **Shift + Enter** → " + tr("select from the last selected row to the currently highlighted row. If none was selected, select from the first row to the highlighted row.") + "\n"
        + "- **Ctrl + A** → " + tr("select all filesystems.") + "\n"
        + "- **Ctrl + C** → " + tr("copy UUIDs of selected filesystems, in sequential order, separated line by line. If none selected, copy the highlighted one.") + "\n\n"

        + tr("## 2. Toolbar navigation") + "\n"
        + tr("- After highlighting a filesystem, press **Enter** or **Space** to focus the toolbar and highlight the first enabled button.") + "\n"
        + tr("- Navigate between toolbar buttons using **→** / **Tab** and **←** / **Shift+Tab**. Navigation wraps around cyclically, skipping disabled buttons.") + "\n"
        + tr("- **↑** has no effect in the toolbar context.") + "\n"
        + tr("- You can escape with both **↓** and **Esc**.") + "\n"
        + tr("- Press **Enter** or **Space** to activate the highlighted toolbar button.") + "\n\n"

        + tr("## 3. Escape and exit") + "\n"
        + tr("- Press **Esc** in the toolbar → return focus to the table.") + "\n"
        + tr("- Press **Esc** in the table:") + "\n"
        + "- " + tr("If there is a selection → clear selection.") + "\n"
        + "- " + tr("If nothing is selected → close the application (**Alt + F4**).") + "\n\n"

        + tr("## 4. Tab behavior") + "\n"
        + tr("- **Tab** / **Shift+Tab** moves the highlight between toolbar buttons when the toolbar is focused.") + "\n"
        + tr("- **Tab** does not change selection in the table; it only changes the highlighted filesystem.") + "\n\n"

        + tr("## 5. Cyclical navigation") + "\n"
        + tr("- Arrow navigation (**↑**/**↓** in table, **→**/**←** in toolbar) wraps around when reaching the first or last item.") + "\n"
        + tr("- Disabled toolbar buttons are skipped during cyclic navigation.") + "\n\n"

        + tr("Summary: The keyboard navigation system separates highlight (hover-like visual feedback) from selection. The table handles moving the highlight and selecting filesystems, while the toolbar handles button activation and cyclic navigation. Tab consolidates horizontal toolbar movement without affecting table selection.");
}

QString
helptexts::what_is_beekeeper_qt()
{
    return tr("# What is beekeeper-qt?") + "\n\n"
        + tr("**beekeeper-qt**, with the help of another program called ``beesd``, scans your entire filesystem for redundancies and duplicate content beyond the file level—at the block level. "
             "beekeeper-qt can remove repeating data patterns inside files, outside files, and even across your whole disk; in other words, it **deduplicates** unnecessary data to free up disk space.") + "\n\n"
        + tr("Combined with *transparent compression* (not yet implemented in beekeeper-qt), it can reduce disk usage by up to 50%, because patterns that commonly repeat in executables get deduplicated. "
             "Wine prefixes used to install Windows programs and games are unified at the disk level, making the prefix overhead practically zero, resulting in more free space.") + "\n\n"
        + tr("To set it up, just press the **Setup** button in the toolbar and hit Enter.") + "\n\n"
        + tr("The first time it runs, depending on how much space is used, your CPU may reach nearly 100%, which will drop once your entire disk has been scanned and deduplicated. "
             "At first, your free space may temporarily decrease because the hash table ``beesd`` uses for deduplication has to be refreshed. "
             "When transparent compression is implemented, a warning will inform you that CPU usage will increase depending on your current disk usage, but during normal use, CPU usage will be minimal—unless you move large amounts of data frequently, in which case, it is recommended to deduplicate only when not actively using your system.") + "\n\n"
        + tr("Press the **+** button to have beekeeper-qt start deduplicating your filesystems on system boot.") + "\n\n"
        + tr("__\"Storage is cheap in the big 2025, they say... Pfft.\"__");
}

QString
helptexts::transparent_compression()
{
    return tr("# Transparent compression and deduplication") + "\n\n"
        + tr("You may have seen this note when setting up a filesystem:") + "\n\n"
        + tr("> *Note: compression only works for new files when they are created. "
              "Run this command to compress your filesystem for the first time.*") + "\n\n"

        + tr("At first this can sound confusing, because **deduplication** and "
              "**transparent compression** work under very different logic. "
              "Although they work together to reduce the used disk space, "
              "they are two separate mechanisms, unrelated to one another, "
              "and not handled by the same utilities.") + "\n\n"

        + tr("In **beekeeper-qt**, as in the filesystem layer itself, they are "
              "completely independent.") + "\n\n"

        + tr("## Deduplication") + "\n\n"
        + tr("This is *beesd*’s job. No matter when you enable or configure "
              "deduplication for a filesystem, *beesd* will **always deduplicate files "
              "that already exist**. It works by:") + "\n\n"
        + "- " + tr("Scanning your disk") + "\n"
        + "- " + tr("Building a hash table of your btrfs filesystem") + "\n"
        + "- " + tr("Finding matching data") + "\n"
        + "- " + tr("Deduplicating identical extents at the block level") + "\n\n"

        + tr("This means deduplication always starts with the data already present "
              "on disk. Even if you delete and recreate the *beesd* configuration, "
              "your existing data remains deduplicated. When you restart the *beesd* "
              "service with the **Start** button, it rebuilds the hash table and looks "
              "for new matching patterns.") + "\n\n"

        + tr("It’s a recursive and additive process, where every run adds more "
              "deduplication opportunities over time as new files are written.") + "\n\n"

        + tr("🢒 You can check if **deduplication** is active by looking at the "
              "**Status** column of the table in beekeeper-qt.") + "\n\n"

        + tr("## Transparent compression") + "\n\n"
        + tr("By contrast, **transparent compression** works in the opposite way: "
              "it compresses **only new files written** to the disk. That means:") + "\n\n"
        + "- " + tr("Existing files remain uncompressed if they were written while "
                   "transparent compression was disabled.") + "\n"
        + "- " + tr("When enabled, the *btrfs* driver uses its own heuristics to "
                   "decide whether new data is compressible.") + "\n"
        + "- " + tr("If compressible, the data is written using the selected compression algorithm "
                   "and level, which you configure at **Setup** time in beekeeper-qt.") + "\n\n"

        + tr("Because of this, transparent compression only makes sense if it’s "
              "**enabled all the time your system is running**. That’s why it is "
              "enabled by default in beekeeper-qt to ensure maximum savings "
              "from the moment files are created.") + "\n\n"

        + tr("🢒 You can check whether **transparent compression** is active for a "
              "filesystem by selecting it with the mouse or keyboard (see **Keyboard "
              "navigation**) and hovering the **zip button**. Its tooltip shows whether "
              "compression is currently running.") + "\n\n"

        + tr("## Gotchas") + "\n\n"
        + tr("- You can start transparent compression manually by pressing the "
              "**zip button**.") + "\n\n"
        + "  - " + tr("This uses the lowest compression level (*feather*) if you didn't setup it.") + "\n"
        + "  - " + tr("However, this setting does **not** persist across reboots, "
                     "so compression will not be active after restarting unless "
                     "enabled in **Setup**.") + "\n\n"

        + tr("### If you only want deduplication at boot:") + "\n\n"
        + "1. " + tr("Remove the setup for your **filesystem(s)**, if not already done.") + "\n"
        + "2. " + tr("Click **Setup**.") + "\n"
        + "3. " + tr("Untick **Enable transparent compression** and press **Enter**.") + "\n\n"
        + tr("This disables automatic compression at boot.") + "\n"
        + tr("*Note: using only **deduplication** is rarely useful on its own because "
              "it will not reduce disk space significantly unless combined with "
              "compression.*") + "\n\n"

        + tr("### If you only want transparent compression:") + "\n\n"
        + "1. " + tr("Remove the setup for your **filesystem(s)**.") + "\n"
        + "2. " + tr("Click **Setup**.") + "\n"
        + "3. " + tr("Set your preferred compression level and press **Enter**.") + "\n"
        + "4. " + tr("If **deduplication** is enabled to start on boot, disable it by pressing the "
                   "**✕** button in the toolbar.") + "\n\n"
        + tr("Compression alone is often more beneficial than deduplication, "
              "but you lose the advantages of combining both.") + "\n\n"

        + tr("### If you want both transparent compression and deduplication:") + "\n\n"
        + "1. " + tr("Remove the setup for your **filesystem(s)**.") + "\n"
        + "2. " + tr("Click **Setup**.") + "\n"
        + "3. " + tr("Set your preferred compression level and press **Enter**.") + "\n"
        + "4. " + tr("Enable deduplication on boot by pressing the **+** button in the toolbar.") + "\n\n"

        + tr("🢒 Using both together can reduce disk usage by up to **50%**, depending "
              "on the type of data you store. Highly repetitive data like logs or "
              "virtual machine images benefits greatly from deduplication, while "
              "general-purpose data (documents, media, source code) sees savings "
              "from compression.") + "\n\n"

        + tr("In summary, transparent compression and deduplication are "
              "complementary features. Deduplication cleans up redundancy in already-"
              "existing data, while compression ensures every new file is written "
              "in the most space-efficient way. Together they maximize space savings "
              "and prolong the life of your storage.") + "\n";
}
