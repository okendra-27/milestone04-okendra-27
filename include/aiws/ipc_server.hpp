#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <thread>
#include <atomic>

namespace aiws {

// POSIX AF_UNIX stream server for the aiws_send wire protocol.
class IpcServer {
public:
    using AddHandler = std::function<void(const std::string& utf8_path)>;

    IpcServer(std::string socket_path, AddHandler handler);
    ~IpcServer();
    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void start();
    void stop() noexcept;
    const std::string& socket_path() const noexcept { return socket_path_; }

private:
    void run();
    std::string socket_path_;
    AddHandler handler_;
    std::thread thread_;
    std::atomic<bool> stopping_{false};
    int listen_fd_{-1};
    std::atomic<int> client_fd_{-1};
    bool owns_path_{false};
};

std::string default_ipc_socket_path();

} // namespace aiws
