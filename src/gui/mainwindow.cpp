// mainwindow.cpp
//
// Main window for the Beekeeper UI. This version updates the filesystem
// table *incrementally* instead of recreating it every refresh. That keeps
// selections intact and is much less disruptive to the user.

#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"

#include "mainwindow.hpp"
#include "statusdotdelegate.hpp"
#include "uuidcolumndelegate.hpp"

#include "help/helpdialog.hpp"
#include "help/texts.hpp"

#include <QApplication>
#include <QFile>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QToolTip>
#include <QVBoxLayout>

using namespace beekeeper::privileged;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // create widgets / objects (same as before)
    fs_table    = new QTableWidget(this);
    refresh_btn = new QPushButton(QIcon::fromTheme("view-refresh"), "");
    start_btn   = new QPushButton(QIcon::fromTheme("media-playback-start"), "");
    stop_btn    = new QPushButton(QIcon::fromTheme("media-playback-stop"), "");
    setup_btn   = new QPushButton(QIcon::fromTheme("system-run"), "");
    compression_switch_btn = new QPushButton(QIcon::fromTheme("package-x-generic"), "");
    compression_switch_btn->setToolTip(tr("Select a filesystem to view its transparent compression status"));
    compression_switch_btn->setCheckable(true);
    compression_switch_btn->setAutoRepeat(false);    // ensure no autorepeat
    add_autostart_btn = new QPushButton(QIcon::fromTheme("list-add"), "");
    remove_autostart_btn = new QPushButton(QIcon::fromTheme("list-remove"), "");
    #ifdef BEEKEEPER_DEBUG_LOGGING
    showlog_btn = new QPushButton(QIcon::fromTheme("text-x-log"), "");
    #endif
    remove_btn = new QPushButton(QIcon::fromTheme("user-trash"), "");
    soft_refresh_timer = new QTimer(this);
    full_refresh_timer = new QTimer(this);

    refresh_btn->setToolTip(tr("Refresh"));
    start_btn->setToolTip(tr("Start"));
    stop_btn->setToolTip(tr("Stop"));
    setup_btn->setToolTip(tr("Setup"));
    add_autostart_btn->setToolTip(tr("Automatically start deduplicating filesystems at boot"));
    remove_autostart_btn->setToolTip(tr("Do not start deduplicating filesystems at boot"));
    #ifdef BEEKEEPER_DEBUG_LOGGING
    showlog_btn->setToolTip(tr("Show logs"));
    #endif

    // ----------------------------
    // SETUP STAGES (only setup, no connections)
    // ----------------------------
    setup_global_menu();
    setup_button_toolbar();
    setup_status_bar();
    setup_fs_table();

    // initialize keyboard navigation (not a connect-style operation)
    keyboardNav = new KeyboardNav(this);
    keyboardNav->init();

    set_temporal_status_message(tr("Loading list..."), 5000);

    // ----------------------------
    // Connect everything and start cycles at the very end
    // ----------------------------
    connect_global_menu_handlers();
    connect_button_toolbar_handlers();
    connect_status_bar_handlers();
    connect_fs_table_handlers();
    start_fs_table_refresh_cycle();
    connect_command_finished_signal();
}

// ---------------------------------------------------------------------
// Stage 1: menu setup + menu actions creation (no connections)
// ---------------------------------------------------------------------
void MainWindow::setup_global_menu()
{
    QMenu *file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->setObjectName("fileMenu");

    // Keep Remove menu action for backward compatibility, but the button is the primary UI.
    QAction *quit_act   = file_menu->addAction(QIcon::fromTheme("application-exit"), tr("Quit"));
    quit_act->setObjectName("actionQuit");

    // --- HELP ---
    QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
    help_menu->setObjectName("helpMenu");

    QAction *keyboard_nav_act = help_menu->addAction(
        QIcon::fromTheme("input-keyboard"),
        tr("Keyboard navigation")
    );
    keyboard_nav_act->setObjectName("actionKeyboardNav");

    QAction *tc_help_act = help_menu->addAction(
        QIcon::fromTheme("package-x-generic"),
        tr("Transparent compression and deduplication")
    );
    tc_help_act->setObjectName("actionTC");

    QAction *about_act = help_menu->addAction(
        QIcon::fromTheme("help-about"),
        tr("About beekeeper-qt")
    );
    about_act->setObjectName("actionAbout");

    // store pointers as members if you need to reference them elsewhere
    this->menu_quit_act = quit_act;
    this->menu_keyboard_nav_act = keyboard_nav_act;
    this->menu_tc_help_act = tc_help_act;
    this->menu_about_act = about_act;
}

// ---------------------------------------------------------------------
// Stage 2: connect global menu handlers (wires the actions)
// ---------------------------------------------------------------------
void MainWindow::connect_global_menu_handlers()
{
    // Quit
    connect(menu_quit_act, &QAction::triggered, this, &QWidget::close);

    // Keyboard navigation help
    connect(menu_keyboard_nav_act, &QAction::triggered, this, [this]() {
        help_dialog *dlg = new help_dialog(
            this,
            tr("About beekeeper-qt"),
            helptexts().keyboardnav()
        );
        dlg->exec();
    });

    // Transparent compression help
    connect(menu_tc_help_act, &QAction::triggered, this, [this]() {
        help_dialog *dlg = new help_dialog(
            this,
            tr("Transparent compression and deduplication"),
            helptexts().transparent_compression()
        );
        dlg->exec();
    });

    // About
    connect(menu_about_act, &QAction::triggered, this, [this]() {
        help_dialog *dlg = new help_dialog(
            this,
            tr("About beekeeper-qt"),
            helptexts().what_is_beekeeper_qt()
        );
        dlg->exec();
    });
}

// ---------------------------------------------------------------------
// Stage 3: toolbar UI construction (no signal connects)
// ---------------------------------------------------------------------
void MainWindow::setup_button_toolbar()
{
    // Create central widget and main vertical layout as members so other
    // setup_* functions can add widgets to the same layout.
    central_widget = new QWidget(this);
    main_layout = new QVBoxLayout(central_widget);

    QHBoxLayout *toolbar = new QHBoxLayout();
    // Correct order: refresh, start, stop, spacing, setup, compression, add/remove autostart, debug, stretch, remove config
    toolbar->addWidget(refresh_btn);
    toolbar->addWidget(start_btn);
    toolbar->addWidget(stop_btn);

    // --- Separator (optional visual, can be just spacing)
    int half_btn_width = stop_btn->sizeHint().width() / 2;
    toolbar->addSpacing(half_btn_width);

    toolbar->addWidget(setup_btn);
    toolbar->addWidget(compression_switch_btn);
    toolbar->addWidget(add_autostart_btn);
    toolbar->addWidget(remove_autostart_btn);

    #ifdef BEEKEEPER_DEBUG_LOGGING
    toolbar->addWidget(showlog_btn);
    #endif

    // stretch pushes the remove_btn to the far right
    toolbar->addStretch();

    // remove configuration button is intended to be on the far right of the toolbar
    toolbar->addWidget(remove_btn);

    // Add toolbar to the main vertical layout — fs_table will be added later to the same layout
    main_layout->addLayout(toolbar);

    // Set the central widget now; fs_table will be added into main_layout by setup_fs_table()
    setCentralWidget(central_widget);

    // Keep a sane default window size
    resize(600, 400);

    // Context menu for Start button (debug only) — creation here, connection in connect_button_toolbar_handlers
    start_btn->setContextMenuPolicy(Qt::CustomContextMenu);
}


// ---------------------------------------------------------------------
// Stage 4: connect toolbar handlers (signal/slot wiring)
// ---------------------------------------------------------------------
void MainWindow::connect_button_toolbar_handlers()
{
    // Timer connect (moved to start_fs_table_refresh_cycle normally, but keep UI button connects here)
    connect(
        refresh_btn,
        &QPushButton::clicked,
        this,
        [this]() {
            refresh_table(true);   // full refresh from daemon
        }
    );
    connect(start_btn,   &QPushButton::clicked, this, &MainWindow::handle_start);
    connect(stop_btn,    &QPushButton::clicked, this, &MainWindow::handle_stop);
    connect(setup_btn,   &QPushButton::clicked, this, &MainWindow::handle_setup);
    connect(compression_switch_btn, &QPushButton::toggled,
            this, [this](bool checked) {
                handle_transparentcompression_switch(checked);
            });

    connect(add_autostart_btn, &QPushButton::clicked, this, &MainWindow::handle_add_to_autostart);
    connect(remove_autostart_btn, &QPushButton::clicked, this, &MainWindow::handle_remove_from_autostart);
    #ifdef BEEKEEPER_DEBUG_LOGGING
    // showlog button handler
    connect(showlog_btn, &QPushButton::clicked, this, &MainWindow::handle_showlog);

    // Context menu for Start button (debug only) — connect the custom context menu now
    connect(start_btn, &QPushButton::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QMenu menu;
        QAction *log_act = menu.addAction(tr("Start with logging enabled"));

        connect(log_act, &QAction::triggered, this, [this]() {
            QString p1 = tr("Logging the Beesd deduplication is very resource intensive and takes a lot of disk space because Beesd logs are massive and only intended for debugging purposes.");
            QString p2 = tr("It is discouraged to enable it by the normal user, hence that's why this is only visible in the Debug release of beekeeper-qt.");
            QString p3 = tr("If you just want to see how much disk space you have freed since you started the service, just hover over a filesystem or select it and look at the status bar, which will show how much free space you had before and how much you have free now.");
            QString p4 = tr("Again, this is purely for debugging purposes and otherwise discouraged to enable.");
            QString p5 = tr("Are you sure you want to continue?");

            QMessageBox::StandardButton reply = QMessageBox::warning(
                this,
                tr("Warning"),
                p1 + "\n\n" + p2 + "\n\n" + p3 + "\n\n" + p4 + "\n\n" + p5,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );

            if (reply == QMessageBox::Yes) {
                handle_start(true);
            }
        });

        menu.exec(start_btn->mapToGlobal(pos));
    });
    #endif

    // Remove configuration button
    remove_btn->setToolTip(tr("Remove configuration file"));
    remove_btn->setEnabled(false); // disabled at startup
    connect(remove_btn, &QPushButton::clicked, this, &MainWindow::handle_remove_button);
}

// ---------------------------------------------------------------------
// Stage 5: status bar setup (no connects)
// ---------------------------------------------------------------------
void MainWindow::setup_status_bar()
{
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    // Mouse hover event for the status bar
    statusBar->setMouseTracking(true);
    statusBar->installEventFilter(this);

    // CPU usage meter on the status bar
    cpu_label = new QLabel("CPU: --%", this);
    cpu_label->setVisible(false);
    statusBar->addPermanentWidget(cpu_label, 0);

    // CPU timer setup
    cpu_timer = new QTimer(this);
    connect(cpu_timer, &QTimer::timeout, this, &MainWindow::handle_cpu_timer);
    cpu_timer->start(500);
}


// ---------------------------------------------------------------------
// status bar connects
// ---------------------------------------------------------------------
void MainWindow::connect_status_bar_handlers()
{
    connect(&statusManager, &DedupStatusManager::status_updated,
            this, &MainWindow::handle_status_updated);

    connect(this, &MainWindow::status_updated,
            this, &MainWindow::handle_status_updated);

    connect(fs_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::update_status_bar);
}

// ---------------------------------------------------------------------
// Stage 6: filesystem table setup (no connects, just construction)
// ---------------------------------------------------------------------
void MainWindow::setup_fs_table()
{
    // FS table initial configuration (same as before)
    fs_table->setColumnCount(3);
    fs_table->setHorizontalHeaderLabels({tr("UUID"), tr("Name"), tr("Dedup status")});
    fs_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    fs_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fs_table->setAlternatingRowColors(true);
    fs_table->setMouseTracking(true); // required, otherwise hover events won’t fire
    fs_table->viewport()->setMouseTracking(true); // very important for hover events
    fs_table->viewport()->installEventFilter(this); // intercept mouse events over the table cells

    UUIDColumnDelegate *uuidDelegate = new UUIDColumnDelegate(fs_table);
    fs_table->setItemDelegateForColumn(0, uuidDelegate);
    // Resize to fit the delegate's preferred size
    int width = uuidDelegate->sizeHint(QStyleOptionViewItem(), QModelIndex()).width();
    fs_table->horizontalHeader()->resizeSection(0, width);

    // Row enumeration
    QHeaderView *vHeader = fs_table->verticalHeader();
    fs_table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Make column width match delegate sizeHint
    fs_table->setItemDelegateForColumn(2, new StatusDotDelegate(fs_table));
    // Stretchable / resizable columns
    fs_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);       // UUID fixed
    fs_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);     // name stretches
    fs_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);       // Status fixed

    // Optional: set initial Status column width to match delegate
    int statusWidth = fs_table->itemDelegateForColumn(2)
                        ->sizeHint(QStyleOptionViewItem(), QModelIndex())
                        .width();
    fs_table->horizontalHeader()->resizeSection(2, statusWidth);

    fs_table->setMouseTracking(true);  // allow hover events to reach the delegate

    fs_table->setSortingEnabled(true); // allow sorting

    // --- IMPORTANT: add the table to the main_layout that was created in setup_button_toolbar()
    if (main_layout) {
        main_layout->addWidget(fs_table);
    } else {
        // Fallback: if for some reason main_layout isn't set, ensure we still set central
        QWidget *central = new QWidget(this);
        QVBoxLayout *tmp_layout = new QVBoxLayout(central);
        tmp_layout->addWidget(fs_table);
        setCentralWidget(central);
    }
}

// ---------------------------------------------------------------------
// filesystem table connects
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
// filesystem table connects
// ---------------------------------------------------------------------
void
MainWindow::connect_fs_table_handlers()
{
    auto *selection_model = fs_table->selectionModel();

    connect(selection_model,
            &QItemSelectionModel::selectionChanged,
            this,
            [this]() {
                // Immediate visual feedback first
                QMetaObject::invokeMethod(
                    this,
                    &MainWindow::update_button_states,
                    Qt::QueuedConnection
                );

                // Heavier / derived state after event loop settles
                QMetaObject::invokeMethod(
                    this,
                    &MainWindow::update_status_bar,
                    Qt::QueuedConnection
                );
            });

    // other table-specific connects can go here
}


// ---------------------------------------------------------------------
// Start the refresh cycle (connects timers & starts them)
// ---------------------------------------------------------------------
void
MainWindow::start_fs_table_refresh_cycle()
{
    // First one is a full refresh
    refresh_table(true);

    // -----------------------------------------------------------------
    // Soft refresh timer (UI coherence, cheap, frequent)
    // -----------------------------------------------------------------
    connect(
        soft_refresh_timer,
        &QTimer::timeout,
        this,
        [this]() {
            refresh_table(false);   // soft refresh
        }
    );

    soft_refresh_timer->start(5000); // every 5s


    // -----------------------------------------------------------------
    // Full refresh timer (truth from daemon, slower)
    // -----------------------------------------------------------------
    connect(
        full_refresh_timer,
        &QTimer::timeout,
        this,
        [this]() {
            refresh_table(true);    // full refresh from daemon
        }
    );

    full_refresh_timer->start(30000); // every 30s
}

// ---------------------------------------------------------------------
// Connect command finished signal to refresh and other housekeeping
// ---------------------------------------------------------------------
void MainWindow::connect_command_finished_signal()
{
    connect(this, &MainWindow::command_finished, this, [this]() {
        DEBUG_LOG("[MainWindow] Root shell ready signal received!");
        refresh_table(false); // first refresh the buttons before doing the full refresh
    });
}
