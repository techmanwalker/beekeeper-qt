#pragma once

#include <QLabel>
#include <QTimer>

class CpuUsageMeter : public QLabel {
    Q_OBJECT

public:
    CpuUsageMeter(QWidget *parent, quint16 refresh_interval = 1000);

    // refresh timer
    QTimer* refresh_timer;

private:
    // consult cpu usage and update label
    void poll ();
};