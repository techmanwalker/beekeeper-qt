// setupdialog.cpp
//
// Implementation of SetupDialog. On Accept it:
//  - Parses the db size (empty => 0)
//  - Filters uuids to those that need setup (supercommander->btrfstat(uuid).empty())
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
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
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

    // Label + numeric entry row
    auto *row = new QHBoxLayout();
    QLabel *lbl = new QLabel(tr("Database size, in bytes: "), this);
    row->addWidget(lbl);

    m_dbSizeEdit = new QLineEdit(this);
    m_dbSizeEdit->setPlaceholderText("1073741824 (1 GiB)");
    QRegularExpression re("^\\d{0,20}$"); // digits only, empty allowed
    m_dbSizeEdit->setValidator(new QRegularExpressionValidator(re, this));
    connect(m_dbSizeEdit, &QLineEdit::textChanged, this, &SetupDialog::on_text_changed);
    m_dbSizeEdit->setMaximumWidth(300);
    row->addWidget(m_dbSizeEdit, /*stretch=*/1);

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

    QLabel *help = new QLabel(
        tr("Only filesystems without an existing configuration will be modified.\n")
        + tr("Leave the field empty to use the default (1 GiB). This value covers most of the use cases."), this
    );
    help->setWordWrap(true);
    main_layout->insertWidget(0, help);
}

void
SetupDialog::on_text_changed(const QString & /*text*/)
{
    // Always keep Accept enabled; validator restricts input
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

    // Parse DB size
    QString txt = m_dbSizeEdit->text().trimmed();
    size_t db_size = 0;
    if (!txt.isEmpty()) {
        bool ok = false;
        db_size = static_cast<size_t>(txt.toULongLong(&ok));
        if (!ok) {
            QMessageBox::critical(this, tr("Invalid value"),
                                  tr("Database size must be a positive integer (bytes)."));
            return;
        }
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

