#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
constexpr unsigned char kMagic[] = {'A', 'I', 'W', 'S'};

int remaining_ms(std::chrono::steady_clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
    return left <= 0 ? 0 : static_cast<int>(left);
}

bool wait_fd(int fd, short events, std::chrono::steady_clock::time_point deadline) {
    pollfd p{fd, events, 0};
    while (remaining_ms(deadline) > 0) {
        const int r = ::poll(&p, 1, remaining_ms(deadline));
        if (r < 0 && errno == EINTR) continue;
        return r > 0 && !(p.revents & (POLLERR | POLLNVAL)) && (p.revents & events);
    }
    return false;
}

bool transfer(int fd, void* data, std::size_t size, bool sending,
              std::chrono::steady_clock::time_point deadline) {
    auto* bytes = static_cast<unsigned char*>(data);
    std::size_t offset = 0;
    while (offset < size) {
        const short ready = sending ? POLLOUT : POLLIN;
        if (!wait_fd(fd, ready, deadline)) return false;
        const ssize_t n = sending
            ? ::send(fd, bytes + offset, size - offset, MSG_NOSIGNAL)
            : ::recv(fd, bytes + offset, size - offset, 0);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) return false;
        offset += static_cast<std::size_t>(n);
    }
    return true;
}

std::string default_socket() {
    const char* user = std::getenv("USER");
    const std::string identity = (user && *user) ? user : std::to_string(::getuid());
    return "/tmp/aiws-" + identity + ".sock";
}
}

int main(int argc, char* argv[]) {
    std::string socket_path = default_socket();
    int i = 1;
    if (i + 1 < argc && std::string(argv[i]) == "--socket") {
        socket_path = argv[i + 1];
        i += 2;
    }
    if (argc - i != 2 || std::string(argv[i]) != "add") {
        std::cerr << "usage: aiws_send [--socket PATH] add FILE\n";
        return 1;
    }
    std::string file_path;
    try { file_path = std::filesystem::absolute(argv[i + 1]).string(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    if (file_path.empty() || file_path.size() > UINT32_MAX ||
        socket_path.size() >= sizeof(sockaddr_un::sun_path)) {
        std::cerr << "path is empty, too long, or invalid\n";
        return 1;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { std::cerr << "socket: " << std::strerror(errno) << '\n'; return 2; }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size() + 1);
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    const int connect_result = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (connect_result < 0 && errno != EINPROGRESS) {
        std::cerr << "connect: " << std::strerror(errno) << '\n'; ::close(fd); return 2;
    }
    const auto connect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    if (connect_result < 0) {
        if (!wait_fd(fd, POLLOUT, connect_deadline)) {
            std::cerr << "connection timed out\n"; ::close(fd); return 2;
        }
        int error = 0; socklen_t length = sizeof(error);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) {
            std::cerr << "connect: " << std::strerror(error ? error : errno) << '\n'; ::close(fd); return 2;
        }
    }
    ::fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    const auto io_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const std::uint32_t wire_size = htonl(static_cast<std::uint32_t>(file_path.size()));
    unsigned char request[sizeof(kMagic) + sizeof(wire_size)];
    std::memcpy(request, kMagic, sizeof(kMagic));
    std::memcpy(request + sizeof(kMagic), &wire_size, sizeof(wire_size));
    if (!transfer(fd, request, sizeof(request), true, io_deadline) ||
        !transfer(fd, file_path.data(), file_path.size(), true, io_deadline)) {
        std::cerr << "request send failed\n"; ::close(fd); return 2;
    }
    unsigned char status = 2;
    std::uint32_t wire_message_size = 0;
    if (!transfer(fd, &status, 1, false, io_deadline) ||
        !transfer(fd, &wire_message_size, sizeof(wire_message_size), false, io_deadline)) {
        std::cerr << "response timed out or communication failed\n"; ::close(fd); return 2;
    }
    const std::uint32_t message_size = ntohl(wire_message_size);
    if (message_size > 65536) { std::cerr << "invalid response\n"; ::close(fd); return 2; }
    std::string message(message_size, '\0');
    if (message_size && !transfer(fd, message.data(), message.size(), false, io_deadline)) {
        std::cerr << "response truncated\n"; ::close(fd); return 2;
    }
    ::close(fd);
    if (status == 0) return 0;
    if (status == 1) { std::cerr << (message.empty() ? "request rejected" : message) << '\n'; return 1; }
    std::cerr << "invalid server response\n";
    return 2;
}
