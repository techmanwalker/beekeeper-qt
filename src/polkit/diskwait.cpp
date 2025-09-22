// diskwait.cpp
#include "diskwait.hpp"

#include "beekeeper/beesdmgmt.hpp"                  // beesstart, autostart, transparentcompression
#include "beekeeper/btrfsetup.hpp"                  // get_mount_paths / get_real_device
#include "beekeeper/transparentcompressionmgmt.hpp"
#include "beekeeper/debug.hpp"
#include "beekeeper/util.hpp"

#include <libudev.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <deque>
#include <unordered_set>
#include <condition_variable>

#include <QtConcurrent/QtConcurrent> // QtConcurrent::run
#include <QFuture>

namespace {

// Aliases to reduce visual clutter
namespace tc = bk_mgmt::transparentcompression;

// Queue + synchronization primitives kept file-local so the background worker and
// the udev event thread can share them safely. Keeping them static here avoids
// changes to the header; we can move them to instance members.
std::deque<std::string> transparentcompression_queue;
std::mutex queue_mutex;

// track which UUIDs we've already autostarted (so we only start beesd once)
std::unordered_set<std::string> autostart_started;
std::mutex autostart_mutex;

// Worker control
QFuture<void> worker_future;
std::atomic_bool worker_stop{false};
std::condition_variable_any worker_cv;

// Helper: push uuid into queue iff not already present
void push_uuid_to_queue(const std::string &uuid)
{
    if (uuid.empty()) return;
    std::lock_guard<std::mutex> lk(queue_mutex);
    for (const auto &u : transparentcompression_queue) {
        if (u == uuid) return;
    }
    transparentcompression_queue.push_back(uuid);
}

// Helper: remove uuid from queue
void remove_uuid_from_queue(const std::string &uuid)
{
    std::lock_guard<std::mutex> lk(queue_mutex);
    auto it = std::remove(transparentcompression_queue.begin(), transparentcompression_queue.end(), uuid);
    if (it != transparentcompression_queue.end())
        transparentcompression_queue.erase(it, transparentcompression_queue.end());
}

// Helper: return snapshot copy of queue
std::vector<std::string> snapshot_queue()
{
    std::lock_guard<std::mutex> lk(queue_mutex);
    return std::vector<std::string>(transparentcompression_queue.begin(), transparentcompression_queue.end());
}

// Helper: process UUIDs already present at startup (mounted before diskwait started).
// For every UUID in /dev/disk/by-uuid, if it's mounted and is_btrfs:
//  - if autostart enabled -> attempt beesstart(uuid) once (honoring autostart_started set)
//  - if transparent compression enabled -> push to queue and call tc::start(uuid)
//
// This mirrors the behavior of the udev 'add' handling so pre-existing mounts are handled.
void process_existing_mounts()
{
    namespace fs = std::filesystem;

    const fs::path by_uuid_dir{"/dev/disk/by-uuid"};
    if (!fs::exists(by_uuid_dir) || !fs::is_directory(by_uuid_dir)) {
        DEBUG_LOG("[diskwait] /dev/disk/by-uuid missing or not accessible - skipping initial scan");
        return;
    }

    for (auto const &entry : fs::directory_iterator(by_uuid_dir)) {
        // filename is the UUID symlink name
        std::string uuid = entry.path().filename().string();
        if (uuid.empty()) continue;
        if (!bk_util::is_uuid(uuid)) continue;

        // Resolve mountpaths (may be multiple)
        std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);
        if (mountpoints.empty()) {
            // not mounted right now
            continue;
        }

        // Ensure at least one mountpoint is btrfs
        bool is_btr = false;
        for (auto const &mp : mountpoints) {
            if (bk_mgmt::is_btrfs(mp)) { is_btr = true; break; }
        }
        if (!is_btr) continue;

        // Autostart: attempt beesstart once (respect autostart_started)
        {
            std::lock_guard<std::mutex> lk(autostart_mutex);
            if (bk_mgmt::autostart::is_enabled_for(uuid) &&
                autostart_started.find(uuid) == autostart_started.end()) {
                DEBUG_LOG("[diskwait] initial-scan: autostart enabled, starting beesd once for uuid:", uuid);
                if (!bk_mgmt::beesstart(uuid)) {
                    DEBUG_LOG("[diskwait] initial-scan: beesstart failed for:", uuid);
                }
                autostart_started.insert(uuid);
            }
        }

        // Transparent compression: queue + try start immediately
        if (tc::is_enabled_for(uuid)) {
            DEBUG_LOG("[diskwait] initial-scan: transparent compression enabled, queueing and starting for:", uuid);
            push_uuid_to_queue(uuid);
            try {
                tc::start(uuid);
            } catch (const std::exception &e) {
                DEBUG_LOG("[diskwait] initial-scan: tc::start threw:", e.what());
            } catch (...) {
                DEBUG_LOG("[diskwait] initial-scan: tc::start threw unknown exception");
            }
        }
    }
}


} // anonymous namespace



// -------------------------------------------------------
// diskwait thread
// -------------------------------------------------------

diskwait::diskwait(QObject *parent)
    : QThread(parent)
{
}

diskwait::~diskwait()
{
    // Signal termination to udev loop (via QThread APIs) and to the background worker
    requestInterruption();

    // Stop the background worker and wait for it to finish
    worker_stop.store(true);
    worker_cv.notify_all();
    if (worker_future.isValid()) {
        worker_future.waitForFinished();
    }

    // Wait for the QThread run() to finish before destroying object
    wait();
}

void
diskwait::run()
{
    DEBUG_LOG("[diskwait] thread started");

    // Start background reconciliation worker (QtConcurrent). It will return immediately
    // and run concurrently. The worker watches the queue and periodically ensures that
    // transparent compression is applied to all mountpoints for queued UUIDs (see below).
    worker_stop.store(false);
    worker_future = QtConcurrent::run([this]() {
        DEBUG_LOG("[diskwait::worker] background worker started");
        using namespace std::chrono_literals;

        while (!worker_stop.load()) {
            // Wait ~30s, but wake earlier if worker_stop is signalled
            for (int i = 0; i < 30 && !worker_stop.load(); ++i) {
                std::this_thread::sleep_for(1s);
            }
            if (worker_stop.load()) break;

            // Snapshot queue to reduce lock hold time
            auto uuids = snapshot_queue();

            for (const auto &uuid : uuids) {
                if (worker_stop.load()) break;

                // If the UUID is no longer mounted at all, remove from queue
                auto mountpoints = bk_mgmt::get_mount_paths(uuid);
                if (mountpoints.empty()) {
                    DEBUG_LOG("[diskwait::worker] uuid no longer mounted, removing from queue:", uuid);
                    remove_uuid_from_queue(uuid);
                    continue;
                }

                // If at least one mount for this uuid has compression running and at least
                // one does not, call start(uuid) to attempt aligning all mounts.
                // This favours the "if some mount has compression, ensure all mounts have it".
                bool any_running = tc::is_running(uuid);
                bool some_not_running = bk_mgmt::transparentcompression::is_not_running_for_at_least_one_mountpoint_of(uuid);

                if (any_running && some_not_running) {
                    DEBUG_LOG("[diskwait::worker] mixed compression state detected for", uuid, "- attempting start()");
                    try {
                        tc::start(uuid); // tc::start is idempotent for already-running mounts
                    } catch (const std::exception &e) {
                        DEBUG_LOG("[diskwait::worker] tc::start threw:", e.what());
                    } catch (...) {
                        DEBUG_LOG("[diskwait::worker] tc::start threw unknown exception");
                    }
                }
            }
        }

        DEBUG_LOG("[diskwait::worker] background worker exiting");
    });

    // process existing mounts that were present before our daemon started
    process_existing_mounts();

    // Setup udev
    struct udev *udev = udev_new();
    if (!udev) {
        DEBUG_LOG("[diskwait] failed to create udev context");
        // make sure worker is stopped
        worker_stop.store(true);
        if (worker_future.isValid()) worker_future.waitForFinished();
        return;
    }

    struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        DEBUG_LOG("[diskwait] failed to create udev monitor");
        udev_unref(udev);
        worker_stop.store(true);
        if (worker_future.isValid()) worker_future.waitForFinished();
        return;
    }

    // Listen only for block device events
    udev_monitor_filter_add_match_subsystem_devtype(mon, "block", nullptr);
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);

    // Main udev loop: react to add/remove/change. For 'add'/'change' we treat as new mount(s).
    while (!isInterruptionRequested()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        timeval tv { 1, 0 }; // timeout: 1s
        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);

        if (ret > 0 && FD_ISSET(fd, &fds)) {
            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (!dev) continue;

            const char *action_c = udev_device_get_action(dev); // "add", "remove", "change", ...
            std::string action = action_c ? action_c : "";

            const char *uuid_cstr = udev_device_get_property_value(dev, "ID_FS_UUID");
            if (uuid_cstr) {
                std::string uuid(uuid_cstr);
                DEBUG_LOG("[diskwait] udev event action=", action, " uuid=", uuid);

                // Only consider valid-looking UUIDs (defensive)
                if (!bk_util::is_uuid(uuid)) {
                    DEBUG_LOG("[diskwait] udev event contained non-UUID, ignoring:", uuid);
                    udev_device_unref(dev);
                    continue;
                }

                // For removal: take uuid out of queue and do not attempt to process it further.
                if (action == "remove") {
                    DEBUG_LOG("[diskwait] remove event - removing uuid from transparent queue:", uuid);
                    remove_uuid_from_queue(uuid);
                    // Note: we intentionally do NOT stop beesd or change autostart. Autostart and beesd lifecycle
                    // are not tied to the mount-unmount events in this design.
                } else {
                    // For add / change / other actions => check mount and take action
                    // 1) ensure it's a btrfs filesystem
                    //    use get_mount_paths to check if it's mounted and is_btrfs
                    auto mountpoints = bk_mgmt::get_mount_paths(uuid);
                    if (mountpoints.empty()) {
                        DEBUG_LOG("[diskwait] event had uuid but no mounts yet, skipping:", uuid);
                        udev_device_unref(dev);
                        continue;
                    }

                    bool is_btr = false;
                    for (const auto &mp : mountpoints) {
                        if (bk_mgmt::is_btrfs(mp)) {
                            is_btr = true;
                            break;
                        }
                    }
                    if (!is_btr) {
                        DEBUG_LOG("[diskwait] not a btrfs fs, skipping:", uuid);
                        udev_device_unref(dev);
                        continue;
                    }

                    // 2) Autostart: only once ever.
                    {
                        std::lock_guard<std::mutex> lk(autostart_mutex);
                        if (bk_mgmt::autostart::is_enabled_for(uuid) &&
                            autostart_started.find(uuid) == autostart_started.end()) {
                            DEBUG_LOG("[diskwait] autostart enabled, starting beesd once for uuid:", uuid);
                            // best-effort start; do not push into autostart_started unless success? We insert regardless
                            // to guarantee only-once behavior (avoid retries on flapping)
                            if (!bk_mgmt::beesstart(uuid)) {
                                DEBUG_LOG("[diskwait] beesstart failed for:", uuid);
                            }
                            autostart_started.insert(uuid);
                        }
                    }

                    // 3) Transparent compression: if enabled for uuid, queue it and attempt to start now.
                    if (tc::is_enabled_for(uuid)) {
                        DEBUG_LOG("[diskwait] transparent compression enabled, queueing and starting for:", uuid);
                        push_uuid_to_queue(uuid);

                        // Try to start compression now (this will remount all mountpoints this UUID resolves to).
                        // tc::start is idempotent for mounts that already have compression.
                        try {
                            tc::start(uuid);
                        } catch (const std::exception &e) {
                            DEBUG_LOG("[diskwait] tc::start threw:", e.what());
                        } catch (...) {
                            DEBUG_LOG("[diskwait] tc::start threw unknown exception");
                        }
                    }
                }
            }

            udev_device_unref(dev);
        }

        // else: timeout, loop again and check interruption
    }

    // Cleanup
    udev_monitor_unref(mon);
    udev_unref(udev);

    // Signal worker to stop and wait for it to finish
    worker_stop.store(true);
    worker_cv.notify_all();
    if (worker_future.isValid()) worker_future.waitForFinished();

    DEBUG_LOG("[diskwait] thread exiting");
}

/*

### Notes / rationale / clarifications

* **Autostart behavior**: `beesstart(uuid)` is called **only once ever** per UUID, even if the device is mounted/unmounted multiple times. That's why `autostart_started` is used to remember we've already invoked it. This matches your requirement (autostart does not need to be re-run for every mount).
* **Transparent compression behavior**: every time a mount event is observed for a UUID that has transparent compression enabled, we:

  * push it to the `transparentcompression_queue` (deduplicated),
  * call `tc::start(uuid)` immediately (best-effort, idempotent),
  * then the background worker periodically (every 30s) re-evaluates queued UUIDs to fix mixed-mount situations.
* **Removal**: when a `remove` udev action is seen for a UUID, it is removed from the queue; the worker also removes UUIDs that become unmounted (no mount paths).
* **Thread-safety & lifecycle**: The concurrent worker is a `QFuture` from `QtConcurrent::run`; graceful shutdown is handled in the destructor by setting `worker_stop` and waiting for `worker_future.waitForFinished()`. The udev loop itself ends when `requestInterruption()`/`wait()` is invoked on the `diskwait` thread (existing pattern).
* **Idempotence**: `tc::start()` and other management calls are assumed idempotent for already-configured/running mounts; this design relies on that (as you requested earlier).

*/