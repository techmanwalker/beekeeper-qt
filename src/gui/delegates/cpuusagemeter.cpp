#include "cpuusagemeter.hpp"
#include "beekeeper/util.hpp"
#include <QDateTime>
#include <qobject.h>

CpuUsageMeter::CpuUsageMeter(QWidget *parent, quint16 refresh_interval) : QLabel(parent)
{
    refresh_timer = new QTimer(this);
    if (refresh_interval != 0) refresh_timer->setInterval(refresh_interval);
    refresh_timer->callOnTimeout([this]() {poll ();});
    poll(); // for the first time
}

void
CpuUsageMeter::poll ()
{
    this->setText(tr("CPU: %1%").arg(
        bk_util::current_cpu_usage(), 
        0,      // field width (0 = no padding)
        'f',        // fixed format (not scientific)
        1        // 1 decimal place
    ));
}