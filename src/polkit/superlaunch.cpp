// superlaunch.cpp

#include "beekeeper/superlaunch.hpp"
#include "../../include/beekeeper/supercommander.hpp"
#include "../polkit/polkit.hpp"           // PolkitManager (Linux only)
#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"

#include "socketoperations.hpp"
using namespace beekeeper::privileged::socketops;

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <signal.h>
#include <poll.h>

using beekeeper::privileged::supercommander;

// small helper to generate a random hex token
static
std::string make_random_token(size_t bytes = 16) {
    std::vector<unsigned char> buf(bytes);
    int fd = ::open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t r = ::read(fd, buf.data(), buf.size());
        (void)r;
        ::close(fd);
    } else {
        // fallback: weaker but okay for dev (not ideal for prod)
        srand((unsigned)time(nullptr) ^ getpid());
        for (size_t i=0;i<bytes;i++) buf[i] = rand() & 0xff;
    }
    static const char hex[] = "0123456789abcdef";
    std::string s; s.reserve(bytes*2);
    for (unsigned char c : buf) {
        s.push_back(hex[c >> 4]);
        s.push_back(hex[c & 0x0f]);
    }
    return s;
}

// ---------- (s, v) ----------
struct PropertyStruct {
    QString key;
    QDBusVariant value;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const PropertyStruct &prop) {
    argument.beginStructure();
    argument << prop.key << prop.value;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, PropertyStruct &prop) {
    argument.beginStructure();
    argument >> prop.key >> prop.value;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(PropertyStruct)
Q_DECLARE_METATYPE(QList<PropertyStruct>)

// ---------- (s, as, b) for ExecStart entries ----------
struct ExecCommand {
    QString path;         // s
    QStringList args;     // as
    bool ignoreFailure;   // b
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const ExecCommand &cmd) {
    argument.beginStructure();
    argument << cmd.path << cmd.args << cmd.ignoreFailure;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, ExecCommand &cmd) {
    argument.beginStructure();
    argument >> cmd.path >> cmd.args >> cmd.ignoreFailure;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(ExecCommand)
Q_DECLARE_METATYPE(QList<ExecCommand>)

// ---------- (s, a(sv)) for auxiliary units ----------
struct AuxUnitStruct {
    QString unitName;
    QList<PropertyStruct> properties;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const AuxUnitStruct &unit) {
    argument.beginStructure();
    argument << unit.unitName << unit.properties;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, AuxUnitStruct &unit) {
    argument.beginStructure();
    argument >> unit.unitName >> unit.properties;
    argument.endStructure();
    return argument;
}

Q_DECLARE_METATYPE(AuxUnitStruct)
Q_DECLARE_METATYPE(QList<AuxUnitStruct>)

namespace {
QString socket_path_for_uid() {
#ifdef __ANDROID__
    return QString("%1/beekeeper-helper-%2.sock")
        .arg(QDir::tempPath())
        .arg(getuid());
#else
    return QString("%1/beekeeper-helper-%2.sock")
        .arg(QDir::tempPath())          // /tmp or /run/user/<uid>
        .arg(getuid());
#endif
}


void registerMetaTypes() {
    static bool registered = false;
    if (!registered) {
        qDBusRegisterMetaType<PropertyStruct>();
        qDBusRegisterMetaType<QList<PropertyStruct>>();

        qDBusRegisterMetaType<ExecCommand>();
        qDBusRegisterMetaType<QList<ExecCommand>>();

        qDBusRegisterMetaType<AuxUnitStruct>();
        qDBusRegisterMetaType<QList<AuxUnitStruct>>();

        registered = true;
    }
}
}

// -------------------- destructor --------------------

superlaunch::~superlaunch() {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_root_shell_unlocked();
}

// -------------------- public API --------------------

bool superlaunch::start_root_shell() {
    std::lock_guard<std::mutex> lock(mtx_);
    return start_root_shell_unlocked();
}

bool superlaunch::stop_root_shell() {
    std::lock_guard<std::mutex> lock(mtx_);
    return stop_root_shell_unlocked();
}

bool superlaunch::root_shell_alive() {
    std::lock_guard<std::mutex> lock(mtx_);
    return root_shell_alive_unlocked();
}

bool superlaunch::root_shell_alive_unlocked() {
    auto& cmd = supercommander::instance();
    return (cmd.root_stdin_fd_ >= 0);
}

supercommander& superlaunch::create_commander() {
    return supercommander::instance();
}

// -------------------- unlocked implementations --------------------

bool
superlaunch::start_root_shell_unlocked() {
    auto& cmd = supercommander::instance();
    if (cmd.root_shell_pid_ > 0 || (cmd.root_stdin_fd_ >= 0 && cmd.root_stdout_fd_ >= 0)) {
        return true;
    }

    registerMetaTypes();

    const QString sockPath = socket_path_for_uid();
    DEBUG_LOG("Using socket path for UID:", sockPath.toStdString());

    // Create Polkit manager and ensure authorization/session
    beekeeper::auth::PolkitManager polkit;

    // --- Use your existing helper ---
    std::string token = make_random_token(32); // e.g. 32 bytes → hex encoded string
    polkit.setLastToken(token);

    QStringList args;
    args << "--socket" << sockPath << "--token" << QString::fromStdString(token);

    // Launch privileged helper via Polkit
    pid_t pid = polkit.runPrivilegedHelper(
        BEEKEEPER_HELPER_PATH, 
        args
    );
    if (pid < 0) {
        qWarning() << "Failed to launch privileged helper via Polkit";
        return false;
    }

    DEBUG_LOG("Privileged helper launched via Polkit");

    // Wait for the helper to create the socket
    const int appear_timeout_ms = 10000;
    QElapsedTimer t; t.start();
    while (t.elapsed() < appear_timeout_ms && !QFileInfo::exists(sockPath)) {
        QThread::msleep(100);
    }

    if (!QFileInfo::exists(sockPath)) {
        qWarning() << "Helper socket did not appear:" << sockPath;
        return false;
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket() failed:" << strerror(errno);
        return false;
    }

    DEBUG_LOG("Attempting to connect to helper socket:", sockPath.toStdString());
    int sockfd = connect_unix_socket(fd, sockPath.toStdString(), 5000);
    if (sockfd < 0) {
        std::cerr << "Failed to connect to helper socket:" << strerror(errno);
        return false;
    }

    DEBUG_LOG("connect() succeeded, fd=", fd);
    DEBUG_LOG("Root shell fds: ", sockfd);
    cmd.set_root_shell_fds(sockfd, sockfd, sockfd);

    std::string receivedToken;
    if (!read_message(sockfd, receivedToken)) {
        qWarning() << "Failed to read token from helper";
        return false;
    }

    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(receivedToken));
    if (!doc.isObject()) { /* fail */ }
    QJsonObject obj = doc.object();
    QString tokenFromHelper = obj["stdout"].toString();

    if (tokenFromHelper.toStdString() != polkit.getLastToken()) {
        qWarning() << "Token mismatch, aborting handshake!\n"
                   << "The token received was: " << tokenFromHelper.toStdString()
                   << "\nExpected token: " << polkit.getLastToken();
        ::close(sockfd);
        return false;
    }

    DEBUG_LOG("Token verified, handshake succeeded");

    DEBUG_LOG("Helper started successfully via Polkit");
    return true;
}


bool
superlaunch::stop_root_shell_unlocked() {
    auto& cmd = supercommander::instance();

    pid_t pid = cmd.root_shell_pid_;

    if (cmd.root_stdin_fd_ >= 0) ::close(cmd.root_stdin_fd_);
    if (cmd.root_stdout_fd_ >= 0 && cmd.root_stdout_fd_ != cmd.root_stdin_fd_) ::close(cmd.root_stdout_fd_);
    if (cmd.root_stderr_fd_ >= 0 && cmd.root_stderr_fd_ != cmd.root_stdin_fd_) ::close(cmd.root_stderr_fd_);

    cmd.set_root_shell_fds(-1, -1, -1);

    if (pid > 0) {
        // Ask nicely
        kill(pid, SIGTERM);

        // Wait up to ~1s
        const int max_ms = 1000;
        int waited = 0;
        while (waited < max_ms) {
            int status = 0;
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (r < 0 && errno != EINTR) break;
            struct timespec ts{0, 100 * 1000 * 1000}; // 100ms
            nanosleep(&ts, nullptr);
            waited += 100;
        }

        // If still alive, shoot it in the head
        if (kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
            (void)waitpid(pid, nullptr, 0);
        }
    }

    cmd.set_root_shell_pid(-1);

    // Extra safety: remove the socket from the parent too
    QString sockPath = socket_path_for_uid();
    QByteArray encodedPath = QFile::encodeName(sockPath);

    // Try unlink first (classic POSIX)
    if (::unlink(encodedPath.constData()) != 0 && errno != ENOENT) {
        qWarning() << "unlink() failed for socket" << sockPath << ":" << strerror(errno);
    }

    // If file still exists, try QFile::remove as fallback
    if (QFile::exists(sockPath)) {
        if (!QFile::remove(sockPath)) {
            qWarning() << "Failed to remove socket using QFile::remove():" << sockPath;
        } else {
            DEBUG_LOG("Removed leftover socket via QFile::remove():", sockPath.toStdString());
        }
    } else {
        DEBUG_LOG("Socket does not exist, nothing to remove:", sockPath.toStdString());
    }

    return true;
}