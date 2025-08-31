// beekeeper-helper.cpp

#include "../../include/beekeeper/internalaliases.hpp"
#include "../../include/beekeeper/supercommander.hpp"
#include "beekeeper/superlaunch.hpp"
#include "beekeeper/debug.hpp"
#include "socketoperations.hpp"
#include "../../include/beekeeper/util.hpp"  // bk_util::exec_command
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>

using namespace beekeeper::privileged::socketops;

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <signal.h>

static volatile bool g_running = true;
void handle_signal(int) { g_running = false; }

static const char* g_socket_path = nullptr;
static void cleanup_socket() { if (g_socket_path) unlink(g_socket_path); }

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[1]) != "--socket" || std::string(argv[3]) != "--token") {
        std::cerr << "Usage: " << argv[0] << " --socket <path> --token <value>\n";
        return 1;
    }
    g_socket_path = argv[2];
    std::string expectedToken = argv[4];
    DEBUG_LOG("[beekeeper-helper] Starting with socket path: ", g_socket_path);
    atexit(cleanup_socket);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Remove stale socket
    unlink(g_socket_path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { std::cerr << "socket() failed: " << strerror(errno) << "\n"; return 1; }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_path, sizeof(addr.sun_path)-1);

    const char* pk_uid = getenv("PKEXEC_UID");
    uid_t client_uid = (pk_uid && *pk_uid) ? static_cast<uid_t>(strtoul(pk_uid, nullptr, 10)) : 0;

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        std::cerr << "bind() failed: " << strerror(errno) << "\n"; return 1;
        exit(1);
    }

    umask(0);
    if (client_uid != 0) {
        // get primary gid for that uid if you want, or just set gid to client_uid too
        // simplest:
        chown(g_socket_path, client_uid, client_uid);
        chmod(g_socket_path, 0600);
    }

    if (listen(fd, 1) < 0) { std::cerr << "listen() failed: " << strerror(errno) << "\n"; return 1; }

    // Ready token for parent
    std::cout << "__BK_READY__\n"; std::cout.flush();

    DEBUG_LOG("[beekeeper-helper] Socket bound and listening at: ", g_socket_path);
    DEBUG_LOG("[beekeeper-helper] Listening for parent connection...");

    DEBUG_LOG("[beekeeper-helper] Waiting for accept...");
    int client_fd = accept(fd, nullptr, nullptr);
    if (client_fd < 0) {
        std::cerr << "accept() failed: " << strerror(errno) << std::endl;
        DEBUG_LOG("[beekeeper-helper] accept() failed: errno=", errno, strerror(errno));
        return 1;
    } else {
        DEBUG_LOG("[beekeeper-helper] Parent connected, fd=", client_fd);
    }

    if (!expectedToken.empty()) {
        // JSON encode the token for the parent
        QJsonObject ready_obj;
        ready_obj["stdout"] = QString::fromStdString(expectedToken); // token goes in stdout
        ready_obj["stderr"] = QString("");
        QJsonDocument ready_doc(ready_obj);
        std::string ready_encoded = ready_doc.toJson(QJsonDocument::Compact).toStdString();

        if (!write_message(client_fd, ready_encoded)) {
            DEBUG_LOG("[beekeeper-helper] Failed to send token over socket");
        } else {
            DEBUG_LOG("[beekeeper-helper] Sent expected token to parent via socket: ", ready_encoded);
        }
    }

    // Main loop: read one command at a time until parent disconnects
    while (g_running) {
        std::string command;
        if (!read_message(client_fd, command)) {
            DEBUG_LOG("[beekeeper-helper] Parent disconnected or read failed");
            break;
        }

        DEBUG_LOG("[beekeeper-helper] Received command: ", command);

        // Execute command and capture stdout/stderr
        command_streams res = bk_util::exec_command(command.c_str());
        DEBUG_LOG("[beekeeper-helper] stdout:", res.stdout_str);
        DEBUG_LOG("[beekeeper-helper] stderr:", res.stderr_str);

        // Encode stdout/stderr in JSON
        QJsonObject out_obj;
        out_obj["stdout"] = QString::fromStdString(res.stdout_str);
        out_obj["stderr"] = QString::fromStdString(res.stderr_str);

        QJsonDocument doc(out_obj);
        std::string encoded = doc.toJson(QJsonDocument::Compact).toStdString();

        // Send encoded message
        write_message(client_fd, encoded);
    }

    close(client_fd);
    close(fd);
    DEBUG_LOG("[beekeeper-helper] Exiting gracefully");
    return 0;
}
