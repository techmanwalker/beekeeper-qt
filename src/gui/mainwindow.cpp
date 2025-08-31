// mainwindow.cpp
//
// Main window for the Beekeeper UI. This version updates the filesystem
// table *incrementally* instead of recreating it every refresh. That keeps
// selections intact and is much less disruptive to the user.

#include "mainwindow.hpp"
#include "setupdialog.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/supercommander.hpp"
#include "statusdotdelegate.hpp"
#include "uuidcolumndelegate.hpp"
#include "../polkit/globals.hpp"

#include <filesystem>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QSet>
#include <QTimer>
#include <QToolTip>

using namespace beekeeper::privileged;
namespace fs = std::filesystem;

// Helper to map raw status (lowercase trimmed) to UI text
static
QString
map_status_text(const QString &raw_status)
{
    QString status = raw_status.trimmed().toLower();

    if (status.startsWith("running")) return "Deduplicating files";
    if (status == "stopped") return "Not running";
    if (status == "failed") return "Failed to run";
    if (status == "unconfigured") return "Not configured";
    if (status.contains("starting")) return "Starting...";
    if (status.contains("stopping")) return "Stopping...";
    return raw_status;
}

// Trim helper: remove everything up to and including the first ':' and trim whitespace
static
std::string
trim_config_path_after_colon(const std::string &raw)
{
    if (raw.empty())
        return "";

    // special-case beekeeperman "no config" message
    if (raw.rfind("No configuration found", 0) == 0)
        return "";

    std::string s = raw;
    auto pos = s.find(':');
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }
    // trim leading
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    // trim trailing
    if (!s.empty()) {
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
    }
    return s;
}

// Return list of explicitly selected UUIDs that actually have a configuration file
QStringList
MainWindow::selected_configured_filesystems() const
{
    QStringList uuids;

    // Only consider explicitly selected rows
    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty())
        return uuids;  // nothing selected, return empty list

    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        std::string path_raw = komander.btrfstat(uuid.toStdString());
        std::string path = trim_config_path_after_colon(path_raw);

        if (!path.empty())
            uuids << uuid;
    }

    return uuids;
}

bool
supercommander_root_check()
{
    return komander.do_i_have_root_permissions();
}

void MainWindow::set_root_thread(root_shell_thread *thread) {
    rootThread = thread;
}

MainWindow::MainWindow(root_shell_thread *thread, QWidget *parent)
    : QMainWindow(parent), rootThread(thread)
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

    connect(refresh_timer, &QTimer::timeout, this, &MainWindow::update_statuses);
    refresh_timer->start(10000); // every 10s

    connect(refresh_btn, &QPushButton::clicked, this, &MainWindow::refresh_filesystems);
    connect(start_btn,   &QPushButton::clicked, this, &MainWindow::handle_start);
    connect(stop_btn,    &QPushButton::clicked, this, &MainWindow::handle_stop);
    connect(setup_btn,   &QPushButton::clicked, this, &MainWindow::handle_setup);

    // selection change should update the remove button state
    connect(fs_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateRemoveButtonState);

    // Start with an initial refresh (will populate the table)
    refresh_filesystems();
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
    fs_table->setHorizontalHeaderLabels({tr("UUID"), tr("Name"), tr("Bees status")});
    fs_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    fs_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fs_table->setAlternatingRowColors(true);

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
    fs_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // Name stretches to fill remaining space
    fs_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); // Status fixed

    // Optional: set initial Status column width to match delegate
    int statusWidth = fs_table->itemDelegateForColumn(2)
                        ->sizeHint(QStyleOptionViewItem(), QModelIndex())
                        .width();
    fs_table->horizontalHeader()->resizeSection(2, statusWidth);


    QMenu *file_menu = menuBar()->addMenu(tr("&File"));

    // Debugging logs (only enable with CMake flag)
    #ifdef BEEKEEPER_DEBUG_LOGGING
    QAction* view_logs_action = new QAction(QIcon::fromTheme("text-x-log"), tr("View debug logs"), this);
    connect(view_logs_action, &QAction::triggered, this, &MainWindow::showDebugLog);
    file_menu->addAction(view_logs_action); // developer-only tool
    #endif

    // Keep Remove menu action for backward compatibility, but the button is the primary UI.
    QAction *quit_act   = file_menu->addAction(QIcon::fromTheme("application-exit"), tr("Quit"));

    // In MainWindow::setup_ui(), add a new toolbar button for Remove configuration
    remove_btn = new QPushButton(QIcon::fromTheme("user-trash"), "");
    remove_btn->setToolTip(tr("Remove configuration file"));
    remove_btn->setEnabled(false); // disabled at startup
    toolbar->addWidget(remove_btn);
    connect(remove_btn, &QPushButton::clicked, this, &MainWindow::handle_remove_button);

    // When root privileged operations are ready, inmediately refresh
    connect(this, &MainWindow::on_root_shell_ready, this, [this]() {
        DEBUG_LOG("[MainWindow] Root shell ready signal received!");
        refresh_filesystems();  // now safe to enable root-only controls
    });

    connect(rootThread, &root_shell_thread::command_finished, this, [this](const QString &cmd, const QString &out){
        DEBUG_LOG("[GUI] Command finished:", cmd.toStdString(), out.toStdString());
        refresh_filesystems(); // update table based on new info
    });

    // connect menu action if used
    connect(quit_act, &QAction::triggered, this, &QWidget::close);
}

// Incremental refresh: update existing rows in-place, add new rows, remove vanished rows.
// This preserves selection and avoids recreating the whole table.
void
MainWindow::refresh_filesystems()
{
    if (!supercommander_root_check()) {
        // If we don't have permissions, clear the table but keep structure
        fs_table->setRowCount(0);
        start_btn->setEnabled(false);
        stop_btn->setEnabled(false);
        setup_btn->setEnabled(false);
        remove_btn->setEnabled(false);
        return;
    }

    // Get authoritative list from komander
    auto filesystems = komander.btrfsls();

    // Build set of UUIDs from the new list for quick lookups
    QSet<QString> incoming_uuids;
    for (const auto &fs : filesystems) {
        QString uuid = QString::fromStdString(fs.at("uuid"));
        incoming_uuids.insert(uuid);
    }

    // Build mapping of current UUID -> row index (current view)
    QMap<QString, int> current_uuid_to_row;
    for (int r = 0; r < fs_table->rowCount(); ++r) {
        QTableWidgetItem *it = fs_table->item(r, 0);
        if (!it) continue;
        QString uuid = it->data(Qt::UserRole).toString();
        if (!uuid.isEmpty()) current_uuid_to_row[uuid] = r;
    }

    // 1) Update existing rows and append new rows
    for (const auto &fs : filesystems) {
        QString uuid  = QString::fromStdString(fs.at("uuid"));
        QString label = QString::fromStdString(fs.count("label") ? fs.at("label") : "");

        // Determine "status" - either stopped/unconfigured/actual beesstatus
        QString status;
        if (!supercommander_root_check()) {
            status = "stopped";
        } else {
            // If no configuration, btrfstat returns empty; treat as unconfigured
            std::string cfg_raw = komander.btrfstat(uuid.toStdString());
            std::string cfg_trimmed = trim_config_path_after_colon(cfg_raw);

            // Treat "No configuration found ..." the same as empty
            if (cfg_trimmed.empty() ||
                cfg_trimmed.find("No configuration found") == 0) {
                status = "unconfigured";
            } else {
                status = QString::fromStdString(komander.beesstatus(uuid.toStdString()));
            }
        }

        QString display_status = map_status_text(status);

        if (current_uuid_to_row.contains(uuid)) {
            // Existing row -> update label and status in-place
            int row = current_uuid_to_row[uuid];
            QTableWidgetItem *name_item = fs_table->item(row, 1);
            if (name_item && name_item->text() != label) {
                name_item->setText(label);
            }

            QTableWidgetItem *status_item = fs_table->item(row, 2);
            if (status_item) {
                // Update display text only if changed, and always update the underlying UserRole raw string
                status_item->setData(Qt::UserRole, status);
                QString current_display = status_item->text();
                if (current_display != display_status) {
                    status_item->setText(display_status);
                }
            } else {
                // defensive: create one if missing
                status_item = new QTableWidgetItem(display_status);
                status_item->setData(Qt::UserRole, status);
                status_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                fs_table->setItem(row, 2, status_item);
            }
        } else {
            // New row -> append
            int row = fs_table->rowCount();
            fs_table->insertRow(row);

            QTableWidgetItem *uuid_item = new QTableWidgetItem();
            uuid_item->setData(Qt::UserRole, uuid);
            uuid_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            QTableWidgetItem *name_item = new QTableWidgetItem(label);
            name_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            QTableWidgetItem *status_item = new QTableWidgetItem(display_status);
            status_item->setData(Qt::UserRole, status);
            status_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            fs_table->setItem(row, 0, uuid_item);
            fs_table->setItem(row, 1, name_item);
            fs_table->setItem(row, 2, status_item);
        }
    }

    // 2) Remove rows that are no longer present (iterate backwards to avoid index shift)
    for (int row = fs_table->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *it = fs_table->item(row, 0);
        if (!it) continue;
        QString uuid = it->data(Qt::UserRole).toString();
        if (!incoming_uuids.contains(uuid)) {
            fs_table->removeRow(row);
        }
    }

    // 3) Compute button enable/disable states
    auto selected_rows = fs_table->selectionModel()->selectedRows();

    // For start/stop/setup: if nothing selected, check all rows (treat-all-as-selected)
    QList<int> rows_to_check;
    if (selected_rows.isEmpty()) {
        for (int r = 0; r < fs_table->rowCount(); ++r)
            rows_to_check.append(r);
    } else {
        for (auto idx : selected_rows)
            rows_to_check.append(idx.row());
    }

    bool any_stopped = false;
    bool any_running = false;
    bool any_unconfigured = false;
    for (int row : rows_to_check) {
        QString status = fs_table->item(row, 2)->data(Qt::UserRole).toString().toLower();
        DEBUG_LOG("Refreshing filesystem " + fs_table->item(row, 0)->data(Qt::UserRole).toString().toLower() + " with status :" + status);
        if (status.startsWith("running")) any_running = true;
        else if (status == "stopped") any_stopped = true;
        else if (status == "unconfigured") any_unconfigured = true;
    }

    start_btn->setEnabled(any_stopped);
    stop_btn->setEnabled(any_running);
    setup_btn->setEnabled(any_unconfigured);

    // For Remove configuration file: only consider explicitly selected rows (do NOT treat-all-as-selected)
    bool any_selected_configured = false;
    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        std::string cfg_raw = komander.btrfstat(uuid.toStdString());
        std::string cfg_trimmed = trim_config_path_after_colon(cfg_raw);
        if (!cfg_trimmed.empty()) {
            any_selected_configured = true;
            break;
        }
    }

    // Re-evaluate delete button enabledness after table update
    updateRemoveButtonState();
}

// populate_table replaced by incremental logic; kept for completeness (not used on refresh)
void
MainWindow::populate_table(const std::vector<std::map<std::string,std::string>>& /*filesystems*/)
{
    // Left empty: incremental refresh updates rows in-place.
    // Kept for API/compatibility but not used.
}

// Return true if any explicitly selected fs is unconfigured
bool
MainWindow::any_selected_unconfigured() const
{
    for (auto idx : fs_table->selectionModel()->selectedRows()) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();
        if (!supercommander_root_check() || komander.btrfstat(uuid.toStdString()).empty()) return true;
    }
    return false;
}

bool
MainWindow::is_running(const QString &raw_status) const
{
    return raw_status.toLower().startsWith("running");
}

void
MainWindow::handle_start()
{
    if (!supercommander_root_check()) return;

    QList<int> rows_to_process;
    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        for (int i = 0; i < fs_table->rowCount(); ++i)
            rows_to_process.append(i);
    } else {
        for (auto idx : selected_rows)
            rows_to_process.append(idx.row());
    }

    for (int row : rows_to_process) {
        QString uuid = fs_table->item(row, 0)->data(Qt::UserRole).toString();
        QString status = fs_table->item(row, 2)->data(Qt::UserRole).toString();
        if (!is_running(status)) {
            fs_table->item(row, 2)->setText("Starting...");
            fs_table->item(row, 2)->setData(Qt::UserRole, QString("starting"));
            komander.beesstart(uuid.toStdString());
        }
    }

    refresh_filesystems();
}

void
MainWindow::handle_stop()
{
    if (!supercommander_root_check()) return;

    QList<int> rows_to_process;
    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        for (int i = 0; i < fs_table->rowCount(); ++i)
            rows_to_process.append(i);
    } else {
        for (auto idx : selected_rows)
            rows_to_process.append(idx.row());
    }

    for (int row : rows_to_process) {
        QString uuid = fs_table->item(row, 0)->data(Qt::UserRole).toString();
        QString status = fs_table->item(row, 2)->data(Qt::UserRole).toString();
        if (is_running(status)) {
            fs_table->item(row, 2)->setText("Stopping...");
            fs_table->item(row, 2)->setData(Qt::UserRole, QString("stopping"));
            komander.beesstop(uuid.toStdString());
        }
    }

    refresh_filesystems();
}

void
MainWindow::handle_setup()
{
    if (!supercommander_root_check()) return;

    QStringList uuids_to_setup;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        // No selection → consider all rows
        for (int row = 0; row < fs_table->rowCount(); ++row)
            uuids_to_setup.append(fs_table->item(row, 0)->data(Qt::UserRole).toString());
    } else {
        // Only selected rows
        for (auto idx : selected_rows)
            uuids_to_setup.append(fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString());
    }

    if (!uuids_to_setup.isEmpty()) {
        // SetupDialog internally filters only unconfigured UUIDs
        SetupDialog dlg(uuids_to_setup, this);
        dlg.exec();
        refresh_filesystems();
    }
}

// ----------- CONFIG REMOVAL LOGIC ------------

// Called when selection changes; only toggles the toolbar remove button.
void
MainWindow::updateRemoveButtonState()
{
    bool any_selected_configured = false;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();

        // Check status to forbid removal of running configs
        QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
        if (status.startsWith("running")) {
            any_selected_configured = false;
            break;
        }

        std::string path = komander.btrfstat(uuid.toStdString());

        // trim everything before the ':' and remove surrounding whitespace
        auto pos = path.find(':');
        if (pos != std::string::npos)
            path = path.substr(pos + 1); // skip the colon itself
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);

        if (!path.empty()) {
            any_selected_configured = true;
            break;
        }
    }

    remove_btn->setEnabled(any_selected_configured);
}

void
MainWindow::handle_remove_button()
{
    if (!supercommander_root_check()) return;

    auto selected_rows = fs_table->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        QMessageBox::information(this, "No selection", "Please select at least one filesystem to remove its configuration.");
        return;
    }

    // Collect selected filesystems that actually have a configuration
    QStringList uuids_to_remove;
    bool blocked_running = false;
    for (auto idx : selected_rows) {
        QString uuid = fs_table->item(idx.row(), 0)->data(Qt::UserRole).toString();

        // Check status to forbid removal of running configs
        QString status = fs_table->item(idx.row(), 2)->data(Qt::UserRole).toString().toLower();
        if (status.startsWith("running")) {
            blocked_running = true;
            DEBUG_LOG("Skipping deletion of running filesystem config: " + uuid.toStdString());
            continue;
        }

        std::string path = komander.btrfstat(uuid.toStdString());

        // trim everything after the ':' and remove whitespaces
        auto pos = path.find(':');
        if (pos != std::string::npos)
            path = path.substr(pos + 1);
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);

        if (!path.empty())
            uuids_to_remove.append(uuid);
    }

    if (blocked_running) {
        QMessageBox::warning(
            this,
            "Forbidden",
            "One or more selected filesystems are being deduplicated.\n"
            "Deleting the configuration of a running deduplication daemon is forbidden."
        );
        return;
    }

    if (uuids_to_remove.isEmpty()) {
        QMessageBox::information(this, "No configuration found", "None of the selected filesystems have a configuration to remove.");
        return;
    }

    // Confirm deletion
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm removal",
        "You're about to remove the file deduplication engine configuration.\n"
        "This does not provoke data loss but you won't have file deduplication functionality\n"
        "unless you set up Beesd again by selecting the filesystem and clicking the Setup button.\n"
        "Are you sure?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) return;

    // Remove each config one at a time
    for (const QString &q : uuids_to_remove) {
        std::string uuid = q.toStdString();
        std::string path = komander.btrfstat(uuid);

        auto pos = path.find(':');
        if (pos != std::string::npos)
            path = path.substr(pos + 1);
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);

        try {
            if (!path.empty() && fs::exists(path))
                fs::remove(path);
        } catch (const fs::filesystem_error &e) {
            qDebug() << "Failed to remove config for" << q << ":" << e.what();
        }
    }

    // Refresh table and statuses
    refresh_filesystems();
}

void
MainWindow::update_statuses()
{
    refresh_filesystems();
}

// Show debug logs (only enabled with CMake flag)
#ifdef BEEKEEPER_DEBUG_LOGGING
#include <QFontDatabase>
#include <QScrollBar>
#include <QTextEdit>

static
QString formatLogLine(const QString &line)
{
    static QRegularExpression re(R"(^(\[[^\]]+\])\s*([^:]+:)(.*)$)");
    auto m = re.match(line);

    if (m.hasMatch()) {
        return QString("<span style='font-weight:bold; color:blue;'>%1</span> "
                       "<span style='font-weight:bold;'>%2</span>%3")
                   .arg(m.captured(1).toHtmlEscaped(),
                        m.captured(2).toHtmlEscaped(),
                        m.captured(3).toHtmlEscaped());
    }
    return line.toHtmlEscaped();
}

void
MainWindow::showDebugLog()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("Beekeeper Debug Log");
    dialog->resize(900, 600);

    auto *layout = new QVBoxLayout(dialog);
    auto *logView = new QTextEdit(dialog);
    logView->setReadOnly(true);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    logView->setFont(monoFont);
    layout->addWidget(logView);

    // Auto-scroll control
    auto *autoScrollButton = new QPushButton("Auto scrolling");
    autoScrollButton->setCheckable(true);
    autoScrollButton->setChecked(true); // enabled by default
    layout->addWidget(autoScrollButton);

    QString logPath = "/tmp/beekeeper-debug.log";
    auto *file = new QFile(logPath, dialog);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        logView->setPlainText("Failed to open debug log at " + logPath);
    } else {
        QTextStream in(file);
        while (!in.atEnd()) {
            logView->append(formatLogLine(in.readLine()));
        }
    }

    // Track whether user scrolled up
    QObject::connect(logView->verticalScrollBar(), &QScrollBar::valueChanged,
                     dialog, [logView, autoScrollButton](int) {
        QScrollBar *sb = logView->verticalScrollBar();
        bool atBottom = (sb->value() == sb->maximum());
        if (!atBottom) {
            autoScrollButton->setChecked(false); // disable auto-scroll
        }
    });

    // Periodically append new log lines
    auto *timer = new QTimer(dialog);
    QObject::connect(timer, &QTimer::timeout, dialog, [file, logView, autoScrollButton]() {
        if (!file->isOpen()) return;
        while (!file->atEnd()) {
            QString line = QString::fromUtf8(file->readLine()).trimmed();
            if (!line.isEmpty()) {
                logView->append(formatLogLine(line));
            }
        }
        // Scroll only if auto-scroll enabled
        if (autoScrollButton->isChecked()) {
            logView->moveCursor(QTextCursor::End);
        }
    });
    timer->start(1000);

    dialog->show();
}
#endif
