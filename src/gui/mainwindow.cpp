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

MainWindow
::MainWindow(QWidget* parent)
{
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
    refresh_timer = new QTimer(this);

    refresh_btn->setToolTip(tr("Refresh"));
    start_btn->setToolTip(tr("Start"));
    stop_btn->setToolTip(tr("Stop"));
    setup_btn->setToolTip(tr("Setup"));
    add_autostart_btn->setToolTip(tr("Automatically start deduplicating filesystems at boot"));
    remove_autostart_btn->setToolTip(tr("Do not start deduplicating filesystems at boot"));
    #ifdef BEEKEEPER_DEBUG_LOGGING
    showlog_btn->setToolTip(tr("Show logs"));
    #endif

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
    connect(compression_switch_btn, &QPushButton::toggled,
    this, [this](bool checked) {
        // checked == pause (button pressed => pause)
        handle_transparentcompression_switch(checked);
    });

    connect(add_autostart_btn, &QPushButton::clicked, this, &MainWindow::handle_add_to_autostart);
    connect(remove_autostart_btn, &QPushButton::clicked, this, &MainWindow::handle_remove_from_autostart);
    #ifdef BEEKEEPER_DEBUG_LOGGING
    connect(showlog_btn, &QPushButton::clicked, this, &MainWindow::handle_showlog);
    #endif
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
    toolbar->addStretch();
    main_layout->addLayout(toolbar);

    #ifdef BEEKEEPER_DEBUG_LOGGING
        // Add context menu to Start button
        start_btn->setContextMenuPolicy(Qt::CustomContextMenu);
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
    file_menu->setObjectName("fileMenu");

    // Keep Remove menu action for backward compatibility, but the button is the primary UI.
    QAction *quit_act   = file_menu->addAction(QIcon::fromTheme("application-exit"), tr("Quit"));

    // --- HELP ---

    // Add the help dialog
    QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
    help_menu->setObjectName("helpMenu");

    // Keyboard navigation action
    QAction *keyboard_nav_act = help_menu->addAction(
        QIcon::fromTheme("input-keyboard"),   // themed keyboard icon
        tr("Keyboard navigation")
    );
      connect(keyboard_nav_act, &QAction::triggered, this, [this]() {
        // Creamos el dialogo con título y mensaje (Markdown)
        help_dialog *dlg = new help_dialog(
            this,
            tr("About beekeeper-qt"),
            helptexts().keyboardnav()
        );
        dlg->exec();
    });

    // Transparent compression and deduplication action
    QAction *tc_help_act = help_menu->addAction(
        QIcon::fromTheme("package-x-generic"),   // archive-like icon
        tr("Transparent compression and deduplication")
    );
    connect(tc_help_act, &QAction::triggered, this, [this]() {
        help_dialog *dlg = new help_dialog(
            this,
            tr("Transparent compression and deduplication"),
            helptexts().transparent_compression()
        );
        dlg->exec();
    });

    // About beekeeper-qt action
    QAction *about_act = help_menu->addAction(
        QIcon::fromTheme("help-about"),       // standard info icon
        tr("About beekeeper-qt")
    );
    connect(about_act, &QAction::triggered, this, [this]() {
        // Creamos el dialogo con título y mensaje (Markdown)
        help_dialog *dlg = new help_dialog(
            this,
            tr("About beekeeper-qt"),
            helptexts().what_is_beekeeper_qt()
        );
        dlg->exec();
    });
    // --- END HELP ---

    // In MainWindow::setup_ui(), add a new toolbar button for Remove configuration
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

    // CPU usage meter on the status bar
    cpu_label = new QLabel("CPU: --%", this);
    statusBar->addPermanentWidget(cpu_label, 0);

    cpu_timer = new QTimer(this);
    connect(cpu_timer, &QTimer::timeout, this, &MainWindow::handle_cpu_timer);
    cpu_timer->start(500); // refresh every 500ms


    // When root privileged operations are ready, inmediately refresh
    connect(this, &MainWindow::root_shell_ready_signal, this, [this]() {
        DEBUG_LOG("[MainWindow] Root shell ready signal received!");
        refresh_filesystems();  // now safe to enable root-only controls
        update_button_states();
        refresh_fs_helpers::update_status_manager(fs_table, statusManager);
    });

    // When a command issued by the buttons already finished, inmediately refresh
    connect(this, &MainWindow::command_finished, this, [this]() {
        DEBUG_LOG("[MainWindow] Root shell ready signal received!");
        refresh_filesystems();  // now safe to enable root-only controls
        update_button_states();
        refresh_fs_helpers::update_status_manager(fs_table, statusManager);
    });

    // connect menu action if used
    connect(quit_act, &QAction::triggered, this, &QWidget::close);

    // update button states every time the table selection changes
    connect(fs_table->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &MainWindow::update_button_states);
}

void
MainWindow::refresh_filesystems()
{
    if (!komander->do_i_have_root_permissions()) {
        fs_table->setRowCount(0);
        start_btn->setEnabled(false);
        stop_btn->setEnabled(false);
        setup_btn->setEnabled(false);
        #ifdef BEEKEEPER_DEBUG_LOGGING
        showlog_btn->setEnabled(false);
        #endif
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
    update_button_states();
}