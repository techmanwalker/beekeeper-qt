#include "dedupstatusmanager.hpp"
#include "beekeeper/util.hpp"
#include "mainwindow.hpp"
#include "tablecheckers.hpp"
#include <QString>
#include <QTimer>

using namespace tablecheckers;

void
DedupStatusManager::set_status(const QString &uuid, const QString &message)
{
    // Emit only if changed to avoid noisy re-emit during refresh
    auto it = status_map.find(uuid);
    if (it != status_map.end() && it.value() == message) {
        // unchanged → no emit
        return;
    }
    status_map[uuid] = message;
    emit status_updated(message);
}

QString
DedupStatusManager::get_status(const QString &hovered_uuid) const
{
    if (!hovered_uuid.isEmpty() && status_map.contains(hovered_uuid))
        return status_map.value(hovered_uuid);

    return QString(); // fallback empty
}

void
MainWindow::set_temporal_status_message(const QString message, qint64 duration_in_ms)
{
    if (!statusBar)
        return;

    // Cache current message
    QString cached_status = statusBar->currentMessage();

    // Show the temporary message
    statusBar->showMessage(message);

    // Schedule restoration after the duration
    QTimer::singleShot(duration_in_ms, this, [this, cached_status, message]() {
        if (!statusBar) return;

        // Only restore if the message wasn't overwritten
        if (statusBar->currentMessage() == message) {
            statusBar->showMessage(cached_status);
        }
    });
}

// CPU usage meter
void
MainWindow::handle_cpu_timer()
{
    bool is_any_beesd_running = is_any(running, fs_table, fs_view_state);

    // hide if no beesd is running
    // only show if any is running
    cpu_label->setVisible(
        is_any_beesd_running
    );

    if (!is_any_beesd_running) return;

    auto future = QtConcurrent::run([]() -> double {
        return bk_util::current_cpu_usage(1);
    });

    QFutureWatcher<double>* watcher = new QFutureWatcher<double>(this);
    connect(watcher, &QFutureWatcher<double>::finished, this, [this, watcher]() {
        double usage = watcher->result();
        cpu_label->setText(QString("CPU: %1%").arg(usage, 0, 'f', 1));
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}
