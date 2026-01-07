#pragma once

// setupdialog.hpp
//
// Small modal dialog used by the GUI to configure DB size for one or more
// selected btrfs filesystems. The dialog only acts on filesystems that
// currently lack a configuration file (it checks via supercommander->btrfstat).
//
// Usage:
//   // Build a list of selected UUIDs in the main window and pass it here.
//   SetupDialog dlg(selectedUuids, this);
//   if (dlg.exec() == QDialog::Accepted) { /* setup completed */ }
//
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QMainWindow>
#include <QStringList>
#include <qtablewidget.h>

class QLineEdit;
class QPushButton;

class SetupDialog : public QDialog
{
    Q_OBJECT
public:
    // uuids: list of selected filesystem UUIDs (may be empty)
    explicit SetupDialog(const QStringList &uuids, QWidget *parent = nullptr);
    ~SetupDialog() override = default;

    QStringList filter_unconfigured_uuids (const QModelIndexList &selected);

private slots:
    // Override accept() so we run the setup operations before closing the dialog.
    void accept() override;

    // Keep the Accept button enabled only when input is valid (digits or empty).
    void on_text_changed(const QString &text);

private:
    QStringList m_uuids;
    QComboBox *m_dbSizeCombo = nullptr;
    QPushButton *m_acceptBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QCheckBox *m_enableCompression = nullptr;
    QComboBox *m_compressionCombo = nullptr;
};
