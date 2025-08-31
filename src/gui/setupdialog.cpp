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
#include <string>

using namespace beekeeper::privileged;

SetupDialog::SetupDialog(const QStringList &uuids, QWidget *parent)
    : QDialog(parent)
    , m_uuids(uuids)
{
    setWindowTitle("Setting up selected filesystems");
    setModal(true);
    setMinimumWidth(420);

    auto *main_layout = new QVBoxLayout(this);

    // Label + numeric entry row
    auto *row = new QHBoxLayout();
    QLabel *lbl = new QLabel("Database size, in bytes: ", this);
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

    m_acceptBtn = new QPushButton("Accept", this);
    m_acceptBtn->setDefault(true);
    m_acceptBtn->setEnabled(true);
    connect(m_acceptBtn, &QPushButton::clicked, this, &SetupDialog::accept);

    m_cancelBtn = new QPushButton("Cancel", this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &SetupDialog::reject);

    btn_row->addWidget(m_acceptBtn);
    btn_row->addWidget(m_cancelBtn);
    main_layout->addLayout(btn_row);

    // Help text
    QLabel *help = new QLabel(
        "Only filesystems without an existing configuration will be modified.\n"
        "Leave the field empty to use the default (1 GiB).", this);
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
    // Ensure root shell is alive
    if (!launcher.root_shell_alive() && !launcher.start_root_shell()) {
        QMessageBox::critical(this, "Error",
                              "Cannot start root shell. Exiting setup.");
        return;
    }

    // Parse DB size (empty => 0)
    QString txt = m_dbSizeEdit->text().trimmed();
    size_t db_size = 0;
    if (!txt.isEmpty()) {
        bool ok = false;
        db_size = static_cast<size_t>(txt.toULongLong(&ok));
        if (!ok) {
            QMessageBox::critical(this, "Invalid value",
                                  "Database size must be a positive integer (bytes).");
            return;
        }
    }

    // Run beessetup for each unconfigured filesystem
    int success_count = 0;
    int failed_count = 0;
    for (const QString &q : m_uuids) {
        std::string uuid = q.toStdString();
        DEBUG_LOG("About to set up filesystem " + uuid + " with db_size of " + std::to_string(db_size));
        if (komander.btrfstat(uuid).find("No configuration found for") != std::string::npos) {
            auto res = komander.beessetup(uuid, db_size);
            if (!res.empty()) success_count++;
            else failed_count++;
        }
    }

    // Optionally, show summary
    if (success_count > 0 || failed_count > 0) {
        QString msg = QString("Setup completed.\nSuccess: %1\nFailed: %2")
                          .arg(success_count)
                          .arg(failed_count);
        QMessageBox::information(this, "Setup summary", msg);
    }

    QDialog::accept();
}
