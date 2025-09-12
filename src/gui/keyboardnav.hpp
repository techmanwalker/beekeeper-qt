#pragma once

#include <QObject>
#include <QKeyEvent>
#include <QPointer>
#include <QTableWidget>
#include <QToolBar>
#include <QStatusBar>
#include <QWidget>

class MainWindow;

class KeyboardNav : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardNav(MainWindow *parent);

    // Install event filters / shortcuts
    void init();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QPointer<MainWindow> mainWindow;

    int keyboard_hover_row = -1;  // Row currently highlighted by keyboard
    int last_selected_row = -1;    // Last explicitly selected row (for Shift+Enter range selection)
    int last_highlighted_row = -1;
    QWidget *last_highlighted_button = nullptr;
    QWidget *current_toolbar_button = nullptr; // currently selected/highlighted toolbar button

    QString status_cache;
    QString last_temp_status; // track last temp message to avoid overwriting

    void moveHover(int delta);         // Move hover up/down
    void selectHover(bool extend_with_shift, bool additive_with_ctrl); // Select current hover
    void highlightRow(int row); // Highlight keyboard hovered row
    void highlightButton(QWidget *btn); // Highlight keyboard hovered button
    void selectAll();
    void activateToolbar();
    void copyUUIDs();
    void update_status_bar();             // Wrapper to call MainWindow::update_status_bar() properly

    QList<QWidget*> toolbarButtons();   // Convenience for navigating buttons
};
