// setupdialog.cpp
//
// Implementation of SetupDialog. On Accept it:
//  - Parses the db size from the combo box (default 256 MiB)
//  - Filters uuids to those that need setup (supercommander->btrfstat(uuid) indicates no config)
//  - Calls supercommander->beessetup(uuid, db_size) for each
//  - Shows a summary (success / failures)
//
// Modal dialog to configure DB size for selected btrfs filesystems.
// Only acts on filesystems without an existing configuration.
// Immediately closes after running setup; GUI refresh handles status updates.

#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "mainwindow.hpp"
#include "../polkit/globals.hpp" // launcher + komander
#include "setupdialog.hpp"

#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <string>

using namespace beekeeper::privileged;

SetupDialog::SetupDialog(const QStringList &uuids, QWidget *parent)
    : QDialog(parent)
    , m_uuids(uuids)
{
    setWindowTitle(tr("Setting up selected filesystems"));
    setModal(true);
    setMinimumWidth(420);

    auto *main_layout = new QVBoxLayout(this);

    // Help label
    QLabel *help = new QLabel(
        tr("Only filesystems without an existing configuration will be modified.\n")
        + tr("Select the database size. This value covers most of the use cases."), this
    );
    help->setWordWrap(true);
    main_layout->addWidget(help);

    // Label + combo box row
    auto *row = new QHBoxLayout();
    QLabel *lbl = new QLabel(tr("Database size: "), this);
    row->addWidget(lbl);

    m_dbSizeCombo = new QComboBox(this);

    // Define fixed database size options
    struct SizeOption {
        size_t value;
        QString display;
    };

    QList<SizeOption> options;

    options.append(SizeOption{16 * 1024 * 1024, tr("16 MiB")});
    options.append(SizeOption{128 * 1024 * 1024, tr("128 MiB")});
    options.append(SizeOption{256 * 1024 * 1024, tr("256 MiB")});
    options.append(SizeOption{1 * 1024 * 1024 * 1024, tr("1 GiB")});
    options.append(SizeOption{4ULL * 1024 * 1024 * 1024, tr("4 GiB")});



    for (const auto &opt : options)
        m_dbSizeCombo->addItem(opt.display, QVariant::fromValue<qulonglong>(opt.value));

    // Set default to 256 MiB
    for (int i = 0; i < m_dbSizeCombo->count(); ++i) {
        if (m_dbSizeCombo->itemData(i).toULongLong() == 256 * 1024 * 1024) {
            m_dbSizeCombo->setCurrentIndex(i);
            break;
        }
    }

    m_dbSizeCombo->setMaximumWidth(300);
    row->addWidget(m_dbSizeCombo, /*stretch=*/1);
    main_layout->addLayout(row);

    // Buttons row
    auto *btn_row = new QHBoxLayout();
    btn_row->addStretch(1);

    m_acceptBtn = new QPushButton(tr("Accept"), this);
    m_acceptBtn->setDefault(true);
    m_acceptBtn->setEnabled(true);
    connect(m_acceptBtn, &QPushButton::clicked, this, &SetupDialog::accept);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &SetupDialog::reject);

    btn_row->addWidget(m_acceptBtn);
    btn_row->addWidget(m_cancelBtn);
    main_layout->addLayout(btn_row);
}

void
SetupDialog::on_text_changed(const QString & /*text*/)
{
    // Always keep Accept enabled; input is restricted by combo box
    m_acceptBtn->setEnabled(true);
}

void
SetupDialog::accept()
{
    if (!launcher->root_alive && !launcher->start_root_shell()) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Cannot start root shell. Exiting setup."));
        return;
    }

    // Parse DB size from combo box
    size_t db_size = 0;
    if (m_dbSizeCombo) {
        db_size = static_cast<size_t>(m_dbSizeCombo->currentData().toULongLong());
    }

    // Filter uuids to only unconfigured filesystems
    QStringList uuids_to_setup;
    for (const QString &q : m_uuids) {
        std::string uuid = q.toStdString();
        if (komander->btrfstat(uuid, "").find("No configuration found for") != std::string::npos)
            uuids_to_setup.append(q);
    }

    if (uuids_to_setup.isEmpty()) {
        QDialog::accept();
        return;
    }

    // Launch all setups asynchronously
    QList<QFuture<std::string>> futures;
    QFutureSynchronizer<std::string> sync;

    for (const QString &q : uuids_to_setup) {
        auto f = komander->async->beessetup(q, db_size);
        futures.append(f);
        sync.addFuture(f);
    }

    // Close dialog immediately
    QDialog::accept();

    // Wait for all futures to finish
    sync.waitForFinished();

    // Count successes and failures
    int success_count = 0;
    int failed_count = 0;
    for (auto &f : futures) {
        std::string result = f.result();
        if (!result.empty() && result.find("No configuration found for") == std::string::npos)
            success_count++;
        else
            failed_count++;
    }

    // Emit finished signal
    MainWindow *mw = qobject_cast<MainWindow*>(parentWidget());
    if (mw && mw->rootThread) {
        QMetaObject::invokeMethod(mw->rootThread,
                                [mw]() { emit mw->rootThread->command_finished("setup", "success", ""); },
                                Qt::QueuedConnection);
    }
}
