#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "../keyboardnav.hpp" // to use KeyboardNav::help_text

class KeyboardNavHelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardNavHelpDialog(QWidget *parent = nullptr);

private:
    void setupTextArea(QTextEdit *text_area);
};
