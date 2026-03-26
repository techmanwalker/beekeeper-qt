#include "barmessage.hpp"
#include <QTimer>

BarMessage::BarMessage(QWidget *parent) : QLabel(parent) {}

void
BarMessage::print (QString message, quint16 hide_after_ms)
{
    setText(message);

    // Run a timer
    if (hide_after_ms > 0)
    QTimer::singleShot (hide_after_ms, [this, message]() {
        // If something else overwrote this message, invalidate the timer
        if (this->text() == message) {
            clear();
        }
    });
}

void
BarMessage::clear ()
{
    print("");
}