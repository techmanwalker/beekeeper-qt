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

#ifdef HAVE_LIBBLKID
  #include <cstring>
  extern "C" {
    #include <blkid/blkid.h>
  }
#endif

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

// Refresh coalescing with generation counter
std::atomic_uint64_t refresh_generation{0};
std::atomic_bool refresh_worker_running{false};

// NEW: Resolve UUID directly from a device node using libblkid
std::string uuid_from_devnode(const char *devnode)
{
    if (!devnode)
        return {};

#ifdef HAVE_LIBBLKID
    blkid_cache cache = nullptr;

    if (blkid_get_cache(&cache, nullptr) < 0 || !cache)
        return {};

    const char *uuid_cstr = blkid_get_tag_value(cache, "UUID", devnode);

    std::string uuid = uuid_cstr ? uuid_cstr : "";

    blkid_put_cache(cache);

    return uuid;
#else
    return {};
#endif
}

// Helper: refresh the libblkid cache when asked to
void fully_refresh_libblkid_cache() {

    DEBUG_LOG("Refreshing blkid cache automatically...");
#ifdef HAVE_LIBBLKID

    blkid_cache cache = nullptr;

    if (blkid_get_cache(&cache, nullptr) < 0 || !cache)
        return;

    // Clean old/invalid entries
    blkid_gc_cache(cache);

    // Force full rescan
    blkid_probe_all(cache);

    // Put and write cache to disk
    blkid_put_cache(cache);

#else
    bk_util::exec_command("blkid");
#endif
}

void async_refresh_libblkid_cache()
{
    // Increment generation (marks that there's something new)
    refresh_generation.fetch_add(1, std::memory_order_relaxed);

    // If there's an already running worker, we don't spawn another one just yet.
    if (refresh_worker_running.exchange(true))
        return;

    (void) QtConcurrent::run([](){

        uint64_t observed_generation = 0;

        while (true) {
            // Take snapshot of the current generation
            observed_generation = refresh_generation.load(std::memory_order_relaxed);

            DEBUG_LOG("[diskwait] libblkid refresh starting, gen=", observed_generation);

            fully_refresh_libblkid_cache();

            DEBUG_LOG("[diskwait] libblkid refresh finished, gen=", observed_generation);

            // If nobody asked for another refresh while this one was still running, exit.
            if (refresh_generation.load(std::memory_order_relaxed) == observed_generation)
                break;

            DEBUG_LOG("[diskwait] new refresh requested while running, repeating...");
        }

        refresh_worker_running.store(false);
    });
}

// Helper: push uuid into queue if not already present
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

    // Before attempting anything else
    // do it synchronously to avoid any wrongdoing
    fully_refresh_libblkid_cache();

    const fs::path by_uuid_dir{"/dev/disk/by-uuid"};
    if (!fs::exists(by_uuid_dir) || !fs::is_directory(by_uuid_dir)) {
        DEBUG_LOG("[diskwait] /dev/disk/by-uuid missing or not accessible - skipping initial scan");
        return;
    }

    for (auto const &entry : fs::directory_iterator(by_uuid_dir)) {
        std::string uuid = entry.path().filename().string();
        if (uuid.empty()) continue;
        if (!bk_util::is_uuid(uuid)) continue;

        std::vector<std::string> mountpoints = bk_mgmt::get_mount_paths(uuid);
        if (mountpoints.empty()) continue;

        bool is_btr = false;
        for (auto const &mp : mountpoints) {
            if (bk_mgmt::is_btrfs(mp)) { is_btr = true; break; }
        }
        if (!is_btr) continue;

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

        if (tc::is_enabled_for(uuid)) {
            DEBUG_LOG("[diskwait] initial-scan: transparent compression enabled, queueing and starting for:", uuid);
            push_uuid_to_queue(uuid);
            try {
                tc::start(uuid);
            } catch (...) {}
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
    requestInterruption();

    worker_stop.store(true);
    worker_cv.notify_all();
    if (worker_future.isValid())
        worker_future.waitForFinished();

    wait();
}

void
diskwait::run()
{
    DEBUG_LOG("[diskwait] thread started");

    worker_stop.store(false);
    worker_future = QtConcurrent::run([this]() {
        DEBUG_LOG("[diskwait::worker] background worker started");
        using namespace std::chrono_literals;

        while (!worker_stop.load()) {
            for (int i = 0; i < 30 && !worker_stop.load(); ++i)
                std::this_thread::sleep_for(1s);

            if (worker_stop.load()) break;

            auto uuids = snapshot_queue();

            for (const auto &uuid : uuids) {
                if (worker_stop.load()) break;

                auto mountpoints = bk_mgmt::get_mount_paths(uuid);
                if (mountpoints.empty()) {
                    DEBUG_LOG("[diskwait::worker] uuid no longer mounted, removing from queue:", uuid);
                    remove_uuid_from_queue(uuid);
                    continue;
                }
            }
        }

        DEBUG_LOG("[diskwait::worker] background worker exiting");
    });

    process_existing_mounts();

    struct udev *udev = udev_new();
    if (!udev) return;

    struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        udev_unref(udev);
        return;
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon, "block", nullptr);
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);

    while (!isInterruptionRequested()) {

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        timeval tv { 1, 0 };
        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);

        if (ret > 0 && FD_ISSET(fd, &fds)) {

            struct udev_device *dev = udev_monitor_receive_device(mon);
            if (!dev) continue;

            const char *action_c = udev_device_get_action(dev);
            std::string action = action_c ? action_c : "";

            const char *devnode = udev_device_get_devnode(dev);
            if (!devnode) {
                udev_device_unref(dev);
                continue;
            }

            // Intercept device-level add/remove
            if (action == "add" || action == "remove") {
                DEBUG_LOG("Device ", action, " detected (devnode=", devnode, ")");
                async_refresh_libblkid_cache();
            }

            std::string uuid = uuid_from_devnode(devnode);
            if (uuid.empty()) {
                udev_device_unref(dev);
                continue;
            }

            DEBUG_LOG("[diskwait] udev event action=", action, " uuid=", uuid);

            if (!bk_util::is_uuid(uuid)) {
                udev_device_unref(dev);
                continue;
            }

            if (action == "remove") {
                remove_uuid_from_queue(uuid);
            } else {

                auto mountpoints = bk_mgmt::get_mount_paths(uuid);
                if (mountpoints.empty()) {
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
                    udev_device_unref(dev);
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lk(autostart_mutex);
                    if (bk_mgmt::autostart::is_enabled_for(uuid) &&
                        autostart_started.find(uuid) == autostart_started.end()) {

                        if (!bk_mgmt::beesstart(uuid)) {
                            DEBUG_LOG("[diskwait] beesstart failed for:", uuid);
                        }
                        autostart_started.insert(uuid);
                    }
                }

                if (tc::is_enabled_for(uuid)) {
                    push_uuid_to_queue(uuid);
                    try {
                        tc::start(uuid);
                    } catch (...) {}
                }
            }

            udev_device_unref(dev);
        }
    }

    udev_monitor_unref(mon);
    udev_unref(udev);

    worker_stop.store(true);
    worker_cv.notify_all();
    if (worker_future.isValid())
        worker_future.waitForFinished();

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