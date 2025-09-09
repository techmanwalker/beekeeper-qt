// mainwindow.cpp
//
// Main window for the Beekeeper UI. This version updates the filesystem
// table *incrementally* instead of recreating it every refresh. That keeps
// selections intact and is much less disruptive to the user.

#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"

#include "mainwindow.hpp"
#include "refreshfilesystems_helpers.hpp"
#include "statusdotdelegate.hpp"
#include "uuidcolumndelegate.hpp"

#include <QApplication>
#include <QFile>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QToolTip>
#include <QVBoxLayout>

using namespace beekeeper::privileged;
namespace fs = std::filesystem;

MainWindow
::MainWindow(QWidget* parent)
{
    fs_table    = new QTableWidget(this);
    refresh_btn = new QPushButton(QIcon::fromTheme("view-refresh"), "");
    start_btn   = new QPushButton(QIcon::fromTheme("media-playback-start"), "");
    stop_btn    = new QPushButton(QIcon::fromTheme("media-playback-stop"), "");
    setup_btn   = new QPushButton(QIcon::fromTheme("system-run"), "");
    refresh_timer = new QTimer(this);

    refresh_btn->setToolTip(tr("Refresh"));
    start_btn->setToolTip(tr("Start"));
    stop_btn->setToolTip(tr("Stop"));
    setup_btn->setToolTip(tr("Setup"));

    setup_ui();

    // Initialize keyboard navigation
    keyboardNav = new KeyboardNav(this);
    keyboardNav->init();

    connect(refresh_timer, &QTimer::timeout, this, &MainWindow::refresh_filesystems);
    refresh_timer->start(10000); // every 10s

    connect(refresh_btn, &QPushButton::clicked, this, &MainWindow::refresh_filesystems);
    connect(start_btn,   &QPushButton::clicked, this, &MainWindow::handle_start);
    connect(stop_btn,    &QPushButton::clicked, this, &MainWindow::handle_stop);
    connect(setup_btn,   &QPushButton::clicked, this, &MainWindow::handle_setup);

    // selection change should update the remove button state
    connect(fs_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::toggle_remove_button_enabled);
}

void
MainWindow::setup_ui()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *main_layout = new QVBoxLayout(central);

    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->addWidget(refresh_btn);
    toolbar->addWidget(start_btn);
    toolbar->addWidget(stop_btn);
    toolbar->addWidget(setup_btn);
    toolbar->addStretch();
    main_layout->addLayout(toolbar);

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

    main_layout->addWidget(fs_table);
    setCentralWidget(central);
    resize(600, 400);

    // Row enumeration
    QHeaderView *vHeader = fs_table->verticalHeader();
    fs_table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Make column width match delegate sizeHint
    fs_table->setItemDelegateForColumn(2, new StatusDotDelegate(fs_table));
    // Stretchable / resizable columns
    fs_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);       // UUID fixed
    fs_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // Initially, take all space
    fs_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); // Status fixed

    // Optional: set initial Status column width to match delegate
    int statusWidth = fs_table->itemDelegateForColumn(2)
                        ->sizeHint(QStyleOptionViewItem(), QModelIndex())
                        .width();
    fs_table->horizontalHeader()->resizeSection(2, statusWidth);

    fs_table->setMouseTracking(true);  // allow hover events to reach the delegate


    QMenu *file_menu = menuBar()->addMenu(tr("&File"));

    // Debugging logs (only enable with CMake flag)
    #ifdef BEEKEEPER_DEBUG_LOGGING
    QAction* view_logs_action = new QAction(QIcon::fromTheme("text-x-log"), tr("View debug logs"), this);
    connect(view_logs_action, &QAction::triggered, this, &MainWindow::showDebugLog);
    file_menu->addAction(view_logs_action); // developer-only tool
    #endif

    // Keep Remove menu action for backward compatibility, but the button is the primary UI.
    QAction *quit_act   = file_menu->addAction(QIcon::fromTheme("application-exit"), tr("Quit"));

    // Add the help dialog
    QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
    QAction *keyboard_nav_act = help_menu->addAction(tr("Keyboard navigation"));
    connect(keyboard_nav_act, &QAction::triggered, this, &MainWindow::show_keyboard_nav_help);

    // In MainWindow::setup_ui(), add a new toolbar button for Remove configuration
    remove_btn = new QPushButton(QIcon::fromTheme("user-trash"), "");
    remove_btn->setToolTip(tr("Remove configuration file"));
    remove_btn->setEnabled(false); // disabled at startup
    toolbar->addWidget(remove_btn);
    connect(remove_btn, &QPushButton::clicked, this, &MainWindow::handle_remove_button);

    // Add the freed-space status bar
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    // Mouse hover event for the status bar
    statusBar->setMouseTracking(true);
    statusBar->installEventFilter(this);
    connect(&statusManager, &DedupStatusManager::status_updated,
            this, &MainWindow::handle_status_updated);

    connect(this, &MainWindow::status_updated,
            this, &MainWindow::handle_status_updated);

    connect(fs_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::update_status_bar);

    // When root privileged operations are ready, inmediately refresh
    connect(this, &MainWindow::root_shell_ready_signal, this, [this]() {
        DEBUG_LOG("[MainWindow] Root shell ready signal received!");
        refresh_filesystems();  // now safe to enable root-only controls
    });

    // connect menu action if used
    connect(quit_act, &QAction::triggered, this, &QWidget::close);
}

void
MainWindow::refresh_filesystems()
{
    if (!komander->do_i_have_root_permissions()) {
        fs_table->setRowCount(0);
        start_btn->setEnabled(false);
        stop_btn->setEnabled(false);
        setup_btn->setEnabled(false);
        remove_btn->setEnabled(false);
        return;
    }

    auto filesystems = komander->btrfsls();
    QSet<QString> incoming_uuids;
    auto current_uuid_map = refresh_fs_helpers::build_current_uuid_map(fs_table);

    // --- build a map uuid -> status using Komander
    QMap<QString, QString> uuid_status_map;
    for (const auto &fs : filesystems) {
        QString uuid = QString::fromStdString(fs.at("uuid"));
        std::string cfg_raw = komander->btrfstat(uuid.toStdString(), "");
        std::string cfg_trimmed = bk_util::trim_config_path_after_colon(cfg_raw);
        QString status;
        if (cfg_trimmed.empty() || cfg_trimmed.find("No configuration found") == 0)
            status = "unconfigured";
        else
            status = QString::fromStdString(bk_util::trim_string(komander->beesstatus(uuid.toStdString())));
        uuid_status_map[uuid] = status;
    }

    // --- update table
    refresh_fs_helpers::update_or_insert_rows(fs_table, filesystems, current_uuid_map, incoming_uuids, uuid_status_map);
    refresh_fs_helpers::remove_vanished_rows(fs_table, incoming_uuids);
    refresh_fs_helpers::update_button_states(fs_table, start_btn, stop_btn, setup_btn, [this](){ toggle_remove_button_enabled(); });
}