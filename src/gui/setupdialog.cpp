// setupdialog.cpp
//
// Implementation of SetupDialog. On Accept it:
//  - Parses the db size from the combo box (default 256 MiB)
//  - Filters uuids to those that need setup (supercommander->btrfstat(uuid) indicates no config)
//  - Calls supercommander->beessetup(uuid, db_size) for each
//  - If compression enabled, configures transparent compression (adds UUID to config)
//    and remounts active filesystems with compression (management::transparentcompression::start)
//  - Shows a summary (success / failures)
//  - Warns the user if compression enabling or remounting fails
//
// Modal dialog to configure DB size and transparent compression
// for selected btrfs filesystems. Only acts on filesystems without
// an existing configuration. Immediately closes after running setup;
// GUI refresh handles status updates.

#include "beekeeper/btrfsetup.hpp"
#include "beekeeper/qt-debug.hpp"
#include "mainwindow.hpp"
#include "tablecheckers.hpp"
#include "../polkit/globals.hpp" // launcher + komander
#include "../polkit/multicommander.hpp"
#include "setupdialog.hpp"

#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QMetaObject>
#include <qabstractitemmodel.h>
#include <qobjectdefs.h>
#include <qwindowdefs.h>
#include <string>

using namespace beekeeper::privileged;
using namespace tablecheckers;


QStringList
SetupDialog::filter_unconfigured_uuids (const QModelIndexList &selected)
{
    QStringList unconfigured_uuids;
    MainWindow *mw = qobject_cast<MainWindow*>(parentWidget());

    for (const QModelIndex &idx : selected) {
        QString uuid = refresh_fs_helpers::fetch_user_role(idx, 0);
        std::string uuid_std = uuid.toStdString();

        // if configured
        if (mw->fs_view_state[uuid_std].status == "unconfigured")
            unconfigured_uuids.push_back(uuid);
    }

    return unconfigured_uuids;
}





SetupDialog::SetupDialog(const QStringList &uuids, QWidget *parent)
    : QDialog(parent)
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

    // Label + combo box row (database size)
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

    // --- Transparent compression section ---
    auto *compression_row = new QHBoxLayout();

    // Tickbox
    m_enableCompression = new QCheckBox(tr("Enable transparent compression"), this);
    m_enableCompression->setChecked(true);
    compression_row->addWidget(m_enableCompression, 1);

    // Compression profile combo (values stored but not currently consumed by management API)
    m_compressionCombo = new QComboBox(this);
    m_compressionCombo->addItem(tr("Feather"),     QVariant("feather"));
    m_compressionCombo->addItem(tr("Light"),       QVariant("light"));
    m_compressionCombo->addItem(tr("Balanced"),    QVariant("balanced"));
    m_compressionCombo->addItem(tr("High"),        QVariant("high"));
    m_compressionCombo->addItem(tr("Harder"),      QVariant("harder"));
    m_compressionCombo->addItem(tr("Maximum"),     QVariant("maximum"));

    // Default = Balanced (index 2)
    m_compressionCombo->setCurrentIndex(2);

    compression_row->addWidget(m_compressionCombo, 1);
    main_layout->addLayout(compression_row);

    // Note label
    QString note_text = tr(
        "Note: compression only works for new files created while it is running.\n"
        "To compress your filesystem for the first time, run:\n"
        "sudo btrfs filesystem defrag -r -czstd mountpoint\n"
        "Pro tip: you can install beekeeper-qt right after installing your Linux to reduce compression overhead from the start."
    );

    QLabel *note = new QLabel(note_text, this);
    QFont f = note->font();
    f.setItalic(true);
    f.setPointSizeF(f.pointSizeF() * 0.85); // smaller
    note->setFont(f);
    note->setWordWrap(true);
    main_layout->addWidget(note);

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
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Cannot start root shell. Exiting setup."));
        return;
    }

    // Parse DB size from combo box
    size_t db_size = 0;
    if (m_dbSizeCombo) {
        db_size = static_cast<size_t>(m_dbSizeCombo->currentData().toULongLong());
    }


    // Fetch selection directly (GUI thread)
    MainWindow *mw = qobject_cast<MainWindow*>(parentWidget());
    if (!mw) {
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("Internal error: no main window."));
        return;
    }

    QModelIndexList selected =
        list_of_selected_rows(mw->fs_table, false);


    // Filter only selected unconfigured filesystems
    QStringList uuids_to_setup = filter_unconfigured_uuids(selected);


    // Collect async futures on heap so they can outlive this dialog
    auto *futures = new QList<QFuture<bool>>();

    // --- Beesd setup ---
    for (const QString &q : uuids_to_setup) {
        futures->append(komander->async->beessetup(q, db_size));
        // Render it as set up
        mw->fs_view_state[q.toStdString()].status = "stopped";
        mw->fs_view_state[q.toStdString()].config = "__DUMMY__";
        // if remove is called and this is still __DUMMY__, remove will need
        // to wait for the next refresh for the actual config path
    }

    // --- Transparent compression ---
    if (m_enableCompression->isChecked()) {
        QString compress_token = m_compressionCombo->currentData().toString();

        for (const QString &q : uuids_to_setup) {
            // Add to transparent compression config
            futures->append(
                komander->async->add_uuid_to_transparentcompression(q, compress_token)
            );

            // Attempt remount/start compression if mounted
            futures->append(
                komander->async->start_transparentcompression_for_uuid(q)
            );

            mw->fs_view_state[q.toStdString()].compressing = true;
        }
    }

    // Close dialog immediately (UI will update once refresh_after_these_futures_finish finishes)
    QDialog::accept();

    // Hand over futures to the main window’s async processor
    if (MainWindow *mw = qobject_cast<MainWindow*>(parentWidget())) {
        mw->refresh_after_these_futures_finish(futures);
    } else {
        // Fallback cleanup if no main window found
        delete futures;
    }
}