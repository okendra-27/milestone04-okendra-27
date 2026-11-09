#include "aiws/gui/main_window.hpp"
#include "aiws/gui/workspace_widget.hpp"
#include "aiws/ipc_server.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <QListWidget>
#include <QProcess>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <thread>
#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#endif

using aiws::gui::MainWindow;

#ifndef _WIN32
static bool send_ipc_request(const std::string& socket_path,
                             const std::string& file_path,
                             bool complete = true) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, socket_path.c_str(), socket_path.size() + 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd); return false;
    }
    const unsigned char magic[] = {'A','I','W','S'};
    const std::uint32_t n = htonl(static_cast<std::uint32_t>(file_path.size()));
    ::send(fd, magic, sizeof(magic), MSG_NOSIGNAL);
    ::send(fd, &n, sizeof(n), MSG_NOSIGNAL);
    if (complete) {
        std::size_t done = 0;
        while (done < file_path.size()) {
            const auto part = std::min<std::size_t>(3, file_path.size() - done);
            const ssize_t wrote = ::send(fd, file_path.data() + done, part, MSG_NOSIGNAL);
            if (wrote <= 0) { ::close(fd); return false; }
            done += static_cast<std::size_t>(wrote);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        unsigned char status = 1;
        std::uint32_t response_size = 0;
        if (::recv(fd, &status, 1, MSG_WAITALL) != 1 ||
            ::recv(fd, &response_size, sizeof(response_size), MSG_WAITALL) != sizeof(response_size)) {
            ::close(fd); return false;
        }
        ::close(fd);
        return status == 0;
    }
    ::close(fd);
    return true;
}
#endif

static int wait_for(const std::function<bool()>& predicate, int timeout = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTimer::singleShot(1, &QCoreApplication::quit);
        QCoreApplication::exec();
    }
    return predicate() ? 0 : 1;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int failures = 0;
    QTemporaryDir dir;
    const QString path = dir.filePath("doc.txt");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    file.write("alpha beta gamma delta");
    file.close();

    {
        MainWindow window;
        window.set_build_delay_ms(100);
        failures += !window.add_document_file(path);
        const auto buttons = window.findChildren<QPushButton*>();
        QPushButton* build = nullptr;
        QPushButton* reset = nullptr;
        for (auto* button : buttons) {
            if (button->text() == "Build Corpus") build = button;
            if (button->text() == "Reset") reset = button;
        }
        if (!build || !reset) return 2;
        const QString path_two = dir.filePath("second.txt");
        QFile file_two(path_two);
        if (!file_two.open(QIODevice::WriteOnly | QIODevice::Text)) return 4;
        file_two.write("second document");
        file_two.close();
        build->click();
        failures += wait_for([&]() { return window.build_status() == "Building corpus..."; }, 100);
        failures += !window.add_document_file(path_two);
        failures += window.findChild<aiws::gui::WorkspaceWidget*>()->corpus_status() != "Corpus not built";
        QTimer::singleShot(350, &app, &QCoreApplication::quit);
        app.exec();
        QCoreApplication::processEvents();
        failures += window.findChild<aiws::gui::WorkspaceWidget*>()->corpus_status() != "Corpus not built";
        reset->click();
        failures += window.findChild<aiws::gui::WorkspaceWidget*>()->corpus_status() != "Corpus: Not built";
        failures += window.build_status() != "Ready";
        failures += window.add_document_file(dir.filePath("missing.txt"));
        failures += build->isEnabled();
    }

#ifndef _WIN32
    {
        const std::string socket_path = dir.filePath("server.sock").toStdString();
        QElapsedTimer shutdown_timer;
        shutdown_timer.start();
        {
            MainWindow window;
            window.start_ipc_server(QString::fromStdString(socket_path));
            auto* documents = window.findChild<QListWidget*>();
            failures += documents == nullptr;
            failures += !send_ipc_request(socket_path, path.toStdString(), false);
            failures += !send_ipc_request(socket_path, path.toStdString());
            failures += wait_for([&]() { return documents && documents->count() == 1; });

            const QString unusual = dir.filePath(QString::fromUtf8("space and café\nline.txt"));
            QFile unusual_file(unusual);
            failures += !unusual_file.open(QIODevice::WriteOnly);
            unusual_file.write("utf8 path");
            unusual_file.close();
            QProcess sender;
            sender.start(QCoreApplication::applicationDirPath() + "/aiws_send",
                         {"--socket", QString::fromStdString(socket_path), "add", unusual});
            failures += !sender.waitForStarted(2000);
            failures += !sender.waitForFinished(7000) || sender.exitCode() != 0;
            failures += wait_for([&]() { return documents && documents->count() == 2; });

            QProcess relative_sender;
            relative_sender.setWorkingDirectory(dir.path());
            relative_sender.start(QCoreApplication::applicationDirPath() + "/aiws_send",
                                  {"--socket", QString::fromStdString(socket_path), "add", "doc.txt"});
            failures += !relative_sender.waitForStarted(2000);
            failures += !relative_sender.waitForFinished(7000) || relative_sender.exitCode() != 0;
            failures += wait_for([&]() { return documents && documents->count() == 3; });

            QProcess rejected_sender;
            rejected_sender.start(QCoreApplication::applicationDirPath() + "/aiws_send",
                                 {"--socket", QString::fromStdString(socket_path), "add",
                                  dir.filePath("not-readable.txt")});
            failures += !rejected_sender.waitForStarted(2000);
            failures += !rejected_sender.waitForFinished(7000) || rejected_sender.exitCode() != 1;

            QProcess unreachable_sender;
            unreachable_sender.start(QCoreApplication::applicationDirPath() + "/aiws_send",
                                     {"--socket", dir.filePath("absent.sock"), "add", path});
            failures += !unreachable_sender.waitForStarted(2000);
            failures += !unreachable_sender.waitForFinished(3000) || unreachable_sender.exitCode() != 2;
        }
        failures += shutdown_timer.elapsed() >= 500;
        failures += QFile::exists(QString::fromStdString(socket_path));
    }
#endif

    {
        MainWindow window;
        failures += !window.add_document_file(path);
        auto* build = [&]() -> QPushButton* {
            for (auto* button : window.findChildren<QPushButton*>())
                if (button->text() == "Build Corpus") return button;
            return nullptr;
        }();
        if (!build) return 3;
        build->click();
        failures += wait_for([&]() {
            for (auto* label : window.findChildren<QLabel*>())
                if (label->text().contains("Corpus: 1 chunks")) return true;
            return false;
        });
    }

    {
        MainWindow window;
        failures += !window.add_document_file(path);
        failures += !window.add_document_file(path); // duplicate ID makes the real worker fail
        for (auto* button : window.findChildren<QPushButton*>())
            if (button->text() == "Build Corpus") button->click();
        failures += wait_for([&]() { return window.build_status() == "Build failed."; });
    }

    return failures == 0 ? 0 : 1;
}
