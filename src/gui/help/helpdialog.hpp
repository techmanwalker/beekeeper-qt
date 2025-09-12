#pragma once

#include <map>
#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "../keyboardnav.hpp" // to use KeyboardNav::help_text

class help_dialog : public QDialog
{
    Q_OBJECT

public:
    help_dialog(QWidget *parent = nullptr, const QString &title = "", const QString &message = "");
private:
    QTextEdit *text_area;
    void setupTextArea(QTextEdit *text_area, const QString &message = "");

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    QString title;
    QString message;
};
