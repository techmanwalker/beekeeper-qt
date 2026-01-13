#pragma once
/**
 * superlaunch.hpp
 *
 * This class is responsible ONLY for lifecycle management of the privileged
 * helper shell. It asks for authorization (Polkit later), forks the root shell,
 * and tears it down when done. It does NOT execute beekeeperman commands —
 * that is exclusively SuperCommander’s job.
 */

#include <atomic>
#include <mutex>
#include <QDBusInterface>
#include <sys/types.h> // for pid_t

// forward-declare the supercommander type so we can return references without including its header
namespace beekeeper { namespace privileged { class supercommander; } }

class superlaunch
{
public:
    superlaunch() = default;            // private constructor
    ~superlaunch() = default;

    superlaunch(const superlaunch&) = delete;
    superlaunch& operator=(const superlaunch&) = delete;

    // Top-level entry points: these acquire the mutex (mtx_) and then call
    // the corresponding _unlocked helper to do actual work without nested locking.
    bool start_root_shell();
    bool root_shell_alive();

    beekeeper::privileged::supercommander& create_commander();

    // Root helper status
    std::atomic_bool root_alive = false;
    std::atomic_bool already_set_root_alive_status = false;

private:

    // _unlocked helpers: do the real work but MUST NOT lock mtx_ (caller locks).
    // These are private to avoid accidental external calls without locking.
    bool root_shell_alive_unlocked();
    bool start_root_shell_unlocked();

    std::mutex mtx_;
};
