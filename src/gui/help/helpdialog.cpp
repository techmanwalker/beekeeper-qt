#include "../keyboardnav.hpp"
#include "helpdialog.hpp"
#include "../mainwindow.hpp"
#include <QString>
#include <QTextBlock>
#include <QTextList>

// Create a dialog that supports Markdown processing,
// and enables two-finger scrolling on touchscreens.
help_dialog::help_dialog(QWidget *parent, const QString &title, const QString &message)
    : QDialog(parent)
{
    this->title = title;
    this->message = message;
    
    setWindowTitle(title);
    setModal(true);
    resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    text_area = new QTextEdit(this);
    text_area->setReadOnly(true);
    layout->addWidget(text_area);

    text_area->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    text_area->viewport()->installEventFilter(this);

    setupTextArea(text_area, message);

    QPushButton *ok_btn = new QPushButton(tr("Accept"), this);
    connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(ok_btn);

    setLayout(layout);
}

void
help_dialog::setupTextArea(QTextEdit *text_area, const QString &message)
{
    text_area->setMarkdown(message);
    text_area->setReadOnly(true);

    // Estilo CSS para listas y spacing
    QString style = R"(
        code {
            font-family: monospace;
            background-color: #f0f0f0;
            padding: 2px 4px;
            border-radius: 4px;
        }
        ul {
            margin-left: 20px;      /* left margin */
            margin-right: 10px;     /* right margin */
            padding-left: 0px;      /* avoid extra padding */
        }
        ul li {
            font-size: 10pt;        /* bullet size */
            line-height: 1.4;       /* line height */
        }
    )";
    text_area->document()->setDefaultStyleSheet(style);
}

bool
help_dialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == text_area->viewport()) {
        if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate) {
            QTouchEvent *touch = static_cast<QTouchEvent *>(event);
            if (touch->points().count() > 1) {
                // Ignore selection if more than one finger
                event->accept();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}