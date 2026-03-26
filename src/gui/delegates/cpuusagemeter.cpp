#include "cpuusagemeter.hpp"
#include "beekeeper/util.hpp"
#include <QDateTime>
#include <qobject.h>

CpuUsageMeter::CpuUsageMeter(QWidget *parent, quint16 refresh_interval) : QLabel(parent)
{
    refresh_timer = new QTimer(this);
    if (refresh_interval != 0) refresh_timer->setInterval(refresh_interval);
    refresh_timer->callOnTimeout([this]() {poll ();});
}

void
CpuUsageMeter::poll ()
{
    this->setText(
        "CPU: " + QString::number(bk_util::current_cpu_usage()) + "%"
    );
}