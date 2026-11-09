#include "aiws/ipc_server.hpp"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace aiws {
namespace {
constexpr unsigned char kMagic[] = {'A', 'I', 'W', 'S'};
constexpr std::uint32_t kMaxPathBytes = 1024 * 1024;

#ifndef _WIN32
bool read_exact(int fd, void* destination, std::size_t count) {
    auto* out = static_cast<unsigned char*>(destination);
    std::size_t offset = 0;
    while (offset < count) {
        pollfd item{fd, POLLIN, 0};
        int ready;
        do { ready = ::poll(&item, 1, 5000); } while (ready < 0 && errno == EINTR);
        if (ready <= 0 || (item.revents & (POLLERR | POLLHUP | POLLNVAL))) return false;
        const ssize_t n = ::recv(fd, out + offset, count - offset, 0);
        if (n == 0) return false;
        if (n < 0) { if (errno == EINTR) continue; return false; }
        offset += static_cast<std::size_t>(n);
    }
    return true;
}

bool write_exact(int fd, const void* source, std::size_t count) {
    auto* data = static_cast<const unsigned char*>(source);
    std::size_t offset = 0;
    while (offset < count) {
        const ssize_t n = ::send(fd, data + offset, count - offset, MSG_NOSIGNAL);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) return false;
        offset += static_cast<std::size_t>(n);
    }
    return true;
}

void reply(int fd, unsigned char status, const std::string& message) {
    const std::uint32_t length = htonl(static_cast<std::uint32_t>(message.size()));
    write_exact(fd, &status, 1);
    write_exact(fd, &length, sizeof(length));
    if (!message.empty()) write_exact(fd, message.data(), message.size());
}
#endif
} // namespace

std::string default_ipc_socket_path() {
#ifdef _WIN32
    return {};
#else
    const char* user = std::getenv("USER");
    std::string identity = (user && *user) ? user : std::to_string(::getuid());
    return "/tmp/aiws-" + identity + ".sock";
#endif
}

IpcServer::IpcServer(std::string socket_path, AddHandler handler)
    : socket_path_(socket_path.empty() ? default_ipc_socket_path() : std::move(socket_path)),
      handler_(std::move(handler)) {}

IpcServer::~IpcServer() { stop(); }

void IpcServer::start() {
#ifdef _WIN32
    throw std::runtime_error("Unix-domain IPC is available only on POSIX systems");
#else
    if (thread_.joinable()) return;
    if (socket_path_.empty() || socket_path_.size() >= sizeof(sockaddr_un::sun_path))
        throw std::invalid_argument("IPC socket path is empty or too long");
    stopping_ = false;
    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket: " + std::string(std::strerror(errno)));
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socket_path_.c_str(), socket_path_.size() + 1);
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        const std::string error = std::strerror(errno);
        ::close(listen_fd_); listen_fd_ = -1;
        throw std::runtime_error("bind " + socket_path_ + ": " + error);
    }
    owns_path_ = true;
    ::chmod(socket_path_.c_str(), S_IRUSR | S_IWUSR);
    if (::listen(listen_fd_, 16) < 0) {
        const std::string error = std::strerror(errno);
        stop();
        throw std::runtime_error("listen: " + error);
    }
    thread_ = std::thread(&IpcServer::run, this);
#endif
}

void IpcServer::stop() noexcept {
#ifndef _WIN32
    stopping_ = true;
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
    }
    const int client = client_fd_.load();
    if (client >= 0) ::shutdown(client, SHUT_RDWR);
    if (thread_.joinable()) thread_.join();
    listen_fd_ = -1;
    if (owns_path_) { ::unlink(socket_path_.c_str()); owns_path_ = false; }
#endif
}

void IpcServer::run() {
#ifndef _WIN32
    while (!stopping_) {
        pollfd item{listen_fd_, POLLIN, 0};
        int ready;
        do { ready = ::poll(&item, 1, 100); } while (ready < 0 && errno == EINTR);
        if (stopping_) break;
        if (ready <= 0) continue;
        const int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0) { if (errno == EINTR || stopping_) continue; else continue; }
        client_fd_ = client;
        if (stopping_) {
            ::close(client);
            client_fd_ = -1;
            break;
        }
        unsigned char magic[sizeof(kMagic)];
        std::uint32_t wire_length = 0;
        if (!read_exact(client, magic, sizeof(magic)) ||
            std::memcmp(magic, kMagic, sizeof(kMagic)) != 0 ||
            !read_exact(client, &wire_length, sizeof(wire_length))) {
            ::close(client); client_fd_ = -1; continue;
        }
        const std::uint32_t length = ntohl(wire_length);
        if (length == 0 || length > kMaxPathBytes) {
            reply(client, 1, "invalid path length"); ::close(client); client_fd_ = -1; continue;
        }
        std::string path(length, '\0');
        if (!read_exact(client, path.data(), path.size())) { ::close(client); client_fd_ = -1; continue; }
        if (path.find('\0') != std::string::npos) {
            reply(client, 1, "path contains a NUL byte"); ::close(client); client_fd_ = -1; continue;
        }
        struct stat info{};
        if (::stat(path.c_str(), &info) < 0 || !S_ISREG(info.st_mode) || ::access(path.c_str(), R_OK) != 0) {
            reply(client, 1, "file is not readable"); ::close(client); client_fd_ = -1; continue;
        }
        try {
            if (handler_) handler_(path);
            reply(client, 0, "accepted");
        } catch (const std::exception& e) {
            reply(client, 1, e.what());
        } catch (...) {
            reply(client, 1, "request rejected");
        }
        ::close(client);
        client_fd_ = -1;
    }
#endif
}

} // namespace aiws
