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