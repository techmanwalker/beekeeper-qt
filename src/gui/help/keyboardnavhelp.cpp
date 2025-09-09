#include "../keyboardnav.hpp"
#include "keyboardnavhelp.hpp"
#include "../mainwindow.hpp"
#include <QString>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextList>

QString KeyboardNav::help_text()
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

KeyboardNavHelpDialog::KeyboardNavHelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Navigation Help"));
    setModal(true);
    resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QTextEdit *text_area = new QTextEdit(this);
    text_area->setReadOnly(true);
    layout->addWidget(text_area);

    setupTextArea(text_area);

    QPushButton *ok_btn = new QPushButton(tr("Accept"), this);
    connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(ok_btn);

    setLayout(layout);
}

void KeyboardNavHelpDialog::setupTextArea(QTextEdit *text_area)
{
    text_area->setMarkdown(KeyboardNav::help_text());
    text_area->setReadOnly(true);

    // Estilo CSS para listas y spacing
    QString style = R"(
        ul {
            margin-left: 20px;      /* left margin */
            margin-right: 10px;     /* right margin */
            padding-left: 0px;      /* avoid extra padding */
        }
        ul li {
            font-size: 10pt;        /* bullet size */
            line-height: 1.4;       /* line height */
        }
    )";
    text_area->document()->setDefaultStyleSheet(style);
}