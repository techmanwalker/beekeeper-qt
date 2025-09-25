#include "refreshfilesystems_helpers.hpp"

#include "keyboardnav.hpp"
#include "mainwindow.hpp"
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenuBar>
#include <QPalette>
#include <QTableWidget>
#include <QToolTip>
#include <QWhatsThis>

KeyboardNav::KeyboardNav(MainWindow *parent)
    : QObject(parent), mainWindow(parent)
{
}

void KeyboardNav::init()
{
    if (!mainWindow) return;

    if (mainWindow->fs_table) {

        mainWindow->fs_table->installEventFilter(this);

        // Ensure table can track hover by keyboard
        mainWindow->fs_table->setFocusPolicy(Qt::StrongFocus);
        mainWindow->fs_table->setSelectionMode(QAbstractItemView::ExtendedSelection);

        // Show initial keyboard hint once
        QString startup_msg = tr("To access the toolbar with the keyboard, select a filesystem and click Enter or Space. More info on Help > Keyboard navigation.");
        mainWindow->statusBar->showMessage(startup_msg);

    }
}

//---------------------------------------------------------
// Core event filter
//---------------------------------------------------------
bool
KeyboardNav::eventFilter(QObject *obj, QEvent *event)
{
    if (!mainWindow) return QObject::eventFilter(obj, event);

    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(obj, event);

    auto *ke = static_cast<QKeyEvent*>(event);

    // Determine current focused widget
    QWidget *focused = mainWindow->focusWidget();
    auto buttons = toolbarButtons();

    // ---------------------------------------------------
    // Global shortcuts
    // ---------------------------------------------------
    if (ke->key() == Qt::Key_F10) {
        QMenuBar *bar = mainWindow->menuBar();
        if (bar && !bar->actions().isEmpty()) {
            // Activate the first menu (usually File)
            bar->setActiveAction(bar->actions().first());
            bar->actions().first()->menu()->popup(bar->mapToGlobal(QPoint(0, bar->height())));
        }
        return true;
    }

    if (ke->modifiers() & Qt::AltModifier) {
        QMenuBar *bar = mainWindow->menuBar();

        if (ke->key() == Qt::Key_F) { // Alt+F
            QAction *file_act = bar->actions().first(); // assuming File is first
            bar->setActiveAction(file_act);
            if (file_act->menu())
                file_act->menu()->popup(bar->mapToGlobal(QPoint(0, bar->height())));
            return true;
        }
        else if (ke->key() == Qt::Key_H) { // Alt+H
            QAction *help_act = bar->actions().last(); // assuming Help is last
            bar->setActiveAction(help_act);
            if (help_act->menu())
                help_act->menu()->popup(bar->mapToGlobal(QPoint(0, bar->height())));
            return true;
        }
    }

    if ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_Q) {
        QApplication::closeAllWindows(); // close app
        return true;
    }

    // ---------------------------------------------------
    // Escape key hierarchy
    // ---------------------------------------------------
    if (ke->key() == Qt::Key_Escape) {
        auto buttons = toolbarButtons();

        // --- Toolbar level ---
        if (!buttons.isEmpty() && (focused && buttons.contains(focused)) || current_toolbar_button) {
            // Clear toolbar highlight
            if (current_toolbar_button) {
                current_toolbar_button->setStyleSheet("");
                current_toolbar_button->update();
                current_toolbar_button = nullptr;
            }

            // Transfer focus to table
            auto table = mainWindow->fs_table;
            if (table) {
                if (keyboard_hover_row < 0 && table->rowCount() > 0)
                    keyboard_hover_row = 0; // first row
                highlightRow(keyboard_hover_row);
                table->setFocus();
            }

            exitToolbar();

            return true;
        }

        // --- Table level ---
        if (obj == mainWindow->fs_table) {
            auto table = mainWindow->fs_table;
            if (!table) return QObject::eventFilter(obj, event);

            QItemSelectionModel *sel = table->selectionModel();
            if (sel->hasSelection()) {
                sel->clearSelection();
                last_selected_row = -1;
                update_status_bar();
                mainWindow->statusBar->showMessage(tr("Press Esc again to exit beekeeper-qt."));
            } else {
                // No selection or already cleared → close app
                QApplication::closeAllWindows(); // Alt+F4 behavior
            }

            return true;
        }
    }

    // ---------------------------------------------------
    // Toolbar button navigation (fully confined)
    // ---------------------------------------------------
    if (!buttons.isEmpty() && (focused && buttons.contains(focused)) || current_toolbar_button) {
        QWidget *current = current_toolbar_button ? current_toolbar_button : focused;
        int idx = buttons.indexOf(current);

        switch (ke->key()) {

        case Qt::Key_Right:
        case Qt::Key_Tab:
            do {
                idx = (idx + 1) % buttons.size(); // wrap around
            } while (!buttons.at(idx)->isEnabled());
            highlightButton(buttons.at(idx));
            return true;

        case Qt::Key_Left:
        case Qt::Key_Backtab:
            do {
                idx = (idx - 1 + buttons.size()) % buttons.size(); // wrap around
            } while (!buttons.at(idx)->isEnabled());
            highlightButton(buttons.at(idx));
            return true;

        case Qt::Key_Up:
            // Up arrow does nothing in toolbar
            return true;

        case Qt::Key_Down:
            // Down arrow acts like Esc: deselect all and focus table
            {
                auto table = mainWindow->fs_table;
                if (current_toolbar_button) {
                    current_toolbar_button->setStyleSheet("");
                    current_toolbar_button->update();
                    current_toolbar_button = nullptr;
                }

                if (table) {
                    if (keyboard_hover_row < 0 && table->rowCount() > 0)
                        keyboard_hover_row = 0; // first row
                    highlightRow(keyboard_hover_row);
                    table->setFocus();

                    // Clear selection if any
                    auto sel = table->selectionModel();
                    if (sel->hasSelection()) {
                        sel->clearSelection();
                        last_selected_row = -1;
                        update_status_bar();
                    }
                }
                exitToolbar();
            }
            return true;

        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
            if (current_toolbar_button && current_toolbar_button->isEnabled()) {
                if (auto *btn = qobject_cast<QPushButton*>(current_toolbar_button))
                    btn->click();
            }
            return true;

        case Qt::Key_Escape:
            // Already handled earlier in eventFilter, no change needed here
            break;

        default:
            break;
        }
    }

    // ---------------------------------------------------
    // Table keyboard navigation
    // ---------------------------------------------------
    if (obj == mainWindow->fs_table) {
        switch (ke->key()) {
        case Qt::Key_Up:
            moveHover(-1);
            return true;
        case Qt::Key_Down:
            moveHover(1);
            return true;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
            selectHover(ke->modifiers() & Qt::ShiftModifier,
                        ke->modifiers() & Qt::ControlModifier);
            activateToolbar(); // move focus & highlight first enabled button
            return true;
            return true;
        case Qt::Key_Left:
        case Qt::Key_Right:
            return true; // prevent table auto-selection
        case Qt::Key_A:
            if (ke->modifiers() & Qt::ControlModifier) {
                selectAll();
                return true;
            }
            break;
        case Qt::Key_C:
            if (ke->modifiers() & Qt::ControlModifier) {
                copyUUIDs();
                return true;
            }
            break;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            activateToolbar();
            return true;
        default:
            break;
        }
    }

    return QObject::eventFilter(obj, event);
}

//---------------------------------------------------------
// Move hover up/down
//---------------------------------------------------------
void KeyboardNav::moveHover(int delta)
{
    auto table = mainWindow->fs_table;
    if (!table) return;

    int rowCount = table->rowCount();
    if (rowCount == 0) return;

    int new_row = keyboard_hover_row;
    if (new_row < 0) new_row = 0;
    else new_row += delta;

    bool hit_edge = false;

    if (new_row < 0) {
        new_row = rowCount - 1; // wrap around
        hit_edge = true;
    } else if (new_row >= rowCount) {
        new_row = 0; // wrap around
        hit_edge = true;
    }

    keyboard_hover_row = new_row;

    // Update visual hover & notify MainWindow
    highlightRow(new_row);
    update_status_bar();

    // Only show toolbar hint if more than 1 row exists
    if (hit_edge && rowCount > 1) {
        mainWindow->set_temporal_status_message(
            tr("Do you want to access the toolbar? Press Enter or Space."),
            2000
        );
    }
}

//---------------------------------------------------------
// Select the current hover row
//---------------------------------------------------------
void KeyboardNav::selectHover(bool shift, bool ctrl)
{
    auto table = mainWindow->fs_table;
    if (!table || keyboard_hover_row < 0) return;

    QItemSelectionModel *sel = table->selectionModel();

    if (shift) {
        // Select from last selected (or first) to current hover
        int start = last_selected_row >= 0 ? last_selected_row : 0;
        int end = keyboard_hover_row;

        if (start > end) std::swap(start, end);
        sel->select(QItemSelection(table->model()->index(start,0),
                                   table->model()->index(end, table->columnCount()-1)),
                    QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    } else if (ctrl) {
        // Add current hover to selection
        sel->select(table->model()->index(keyboard_hover_row, 0),
                    QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    } else {
        // Replace selection
        sel->clearSelection();
        sel->select(table->model()->index(keyboard_hover_row,0),
                    QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }

    last_selected_row = keyboard_hover_row;

    // --- NEW: update status like mouse hover does ---
    // If exactly one row is selected, show that row's "Deduplicating files..." message
    int sel_count = refresh_fs_helpers::selected_rows_count(table);
    if (sel_count == 1) {
        // Find the UUID for the selected/hovered row
        QTableWidgetItem *uuid_item = table->item(keyboard_hover_row, 0);
        QString uuid;
        if (uuid_item) uuid = uuid_item->data(Qt::UserRole).toString();

        if (!uuid.isEmpty()) {
            // Set current hovered uuid in the main window (so other code can use it)
            mainWindow->current_hovered_uuid = uuid;

            // Fast-path update for single UUID (same as mouse hover path)
            refresh_fs_helpers::update_status_manager_one_uuid(
                table,
                mainWindow->statusManager,
                uuid
            );

            // Get and display the status
            QString status = mainWindow->statusManager.get_status(uuid);
            if (!status.isEmpty()) {
                mainWindow->statusBar->showMessage(status);
            } else {
                // fallback: clear message if no status
                mainWindow->statusBar->clearMessage();
            }
            return;
        }
    }

    // If multiple selected or nothing to show, use the normal selection-based status update
    mainWindow->current_hovered_uuid.clear();
    update_status_bar();
}

// Highlight keyboard hovered row
void KeyboardNav::highlightRow(int row)
{
    auto table = mainWindow->fs_table;
    if (!table) return;

    // Reset previous highlight
    if (last_highlighted_row >= 0 && last_highlighted_row < table->rowCount()) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem *item = table->item(last_highlighted_row, col);
            if (item) {
                item->setBackground(Qt::NoBrush);
            }
        }
    }

    if (row < 0 || row >= table->rowCount()) return;

    QPalette pal = table->palette();
    QPalette inactivePal = pal;
    inactivePal.setCurrentColorGroup(QPalette::Inactive);
    QColor highlight = inactivePal.color(QPalette::Highlight); // lighter for inactive effect

    for (int col = 0; col < table->columnCount(); ++col) {
        QTableWidgetItem *item = table->item(row, col);
        if (item) {
            item->setBackground(highlight);
        }
    }

    last_highlighted_row = row;
}

// Highlight keyboard hovered button
void KeyboardNav::highlightButton(QWidget *btn)
{
    if (!mainWindow || !btn) return;

    // Reset all toolbar buttons first
    auto buttons = toolbarButtons();
    for (auto *b : buttons) {
        b->setStyleSheet("");
        b->update();
    }

    // Apply highlight style
    QColor highlight = mainWindow->fs_table->palette().color(QPalette::Highlight);
    btn->setStyleSheet(QString("background-color: %1").arg(highlight.name()));
    btn->update();

    // Update current button
    current_toolbar_button = btn;

    // Now show tooltip for current button
    QToolTip::showText(btn->mapToGlobal(QPoint(btn->width()/2, btn->height()/2)), btn->toolTip(), btn);

    // Clear any table highlight
    if (last_highlighted_row >= 0 && mainWindow->fs_table) {
        for (int col = 0; col < mainWindow->fs_table->columnCount(); ++col) {
            QTableWidgetItem *item = mainWindow->fs_table->item(last_highlighted_row, col);
            if (item) item->setBackground(Qt::NoBrush);
        }
        last_highlighted_row = -1;
    }
}

void KeyboardNav::exitToolbar()
{
    // Hide tooltip for all toolbar buttons explicitly
    for (auto *btn : toolbarButtons()) {
        if (!btn) continue;

        // Standard API: clear any tooltip currently showing for this widget
        QToolTip::hideText();

        // Also reset visual highlight if it’s the active one
        btn->setStyleSheet("");
        btn->update();
    }

    current_toolbar_button = nullptr;
}


//---------------------------------------------------------
// Select all rows
//---------------------------------------------------------
void KeyboardNav::selectAll()
{
    auto table = mainWindow->fs_table;
    if (!table) return;

    table->selectAll();
    last_selected_row = keyboard_hover_row;
    update_status_bar();
}

//---------------------------------------------------------
// Move focus to toolbar
//---------------------------------------------------------
void KeyboardNav::activateToolbar()
{
    auto buttons = toolbarButtons();
    if (buttons.isEmpty()) return;

    // Focus first enabled button
    for (auto *btn : buttons) {
        if (btn->isEnabled()) {
            // Highlight immediately and set focus
            highlightButton(btn);
            break;
        }
    }
}

//---------------------------------------------------------
// Copy UUIDs to clipboard
//---------------------------------------------------------
void KeyboardNav::copyUUIDs()
{
    auto table = mainWindow->fs_table;
    if (!table) return;

    QStringList uuids;

    auto sel_rows = table->selectionModel()->selectedRows();
    if (!sel_rows.isEmpty()) {
        for (auto idx : sel_rows) {
            QString uuid = table->item(idx.row(),0)->data(Qt::UserRole).toString();
            uuids << uuid;
        }
    } else if (keyboard_hover_row >= 0) {
        QString uuid = table->item(keyboard_hover_row,0)->data(Qt::UserRole).toString();
        uuids << uuid;
    }

    QApplication::clipboard()->setText(uuids.join("\n"));
    mainWindow->statusBar->showMessage(tr("UUID(s) copied to clipboard."));
}

//---------------------------------------------------------
// Wrapper to update status bar in MainWindow
//---------------------------------------------------------
void KeyboardNav::update_status_bar()
{
    if (!mainWindow) return;

    mainWindow->update_status_bar();
}

//---------------------------------------------------------
// Convenience: toolbar buttons
//---------------------------------------------------------
QList<QWidget*> KeyboardNav::toolbarButtons()
{
    QList<QWidget*> buttons;
    if (!mainWindow) return buttons;

    // Include all buttons including autostart
    buttons << mainWindow->refresh_btn
            << mainWindow->start_btn
            << mainWindow->stop_btn
            << mainWindow->setup_btn
            << mainWindow->compression_switch_btn
            << mainWindow->add_autostart_btn
            << mainWindow->remove_autostart_btn
            #ifdef BEEKEEPER_DEBUG_LOGGING
            << mainWindow->showlog_btn
            #endif
            << mainWindow->remove_btn;

    return buttons;
}
