#pragma once

#include <QLabel>

class BarMessage : public QLabel {
    Q_OBJECT

public:
    BarMessage(QWidget *parent = nullptr);
    
    void print (QString message, quint16 hide_after_ms = 0);
    void clear (); // set the text to ""
};