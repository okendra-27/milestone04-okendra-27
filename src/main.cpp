#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QStringList>
#include <exception>

// TODO: Define the ExitCallback function used to confirm an Exit request.

void ExitCallback(QWidget* parent) {
    QMessageBox::StandardButton response =
        QMessageBox::question(
            parent,
            "Confirm Exit",
            "Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (response == QMessageBox::Yes) {
        QApplication::quit();
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    aiws::gui::MainWindow window;
    const QStringList args = app.arguments();
    QString ipc_socket;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--build-delay-ms" && i + 1 < args.size()) {
            bool ok = false;
            const int delay = args[++i].toInt(&ok);
            if (ok) window.set_build_delay_ms(delay);
        } else if (args[i] == "--ipc-socket") {
            if (i + 1 >= args.size()) {
                QMessageBox::critical(nullptr, "IPC startup failed",
                                      "--ipc-socket requires a socket path.");
                return 2;
            }
            ipc_socket = args[++i];
        }
    }

    try {
        window.start_ipc_server(ipc_socket);
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "IPC startup failed", e.what());
        return 1;
    }

    // Register required callback
    window.set_exit_callback(ExitCallback);

    // M3 startup size requirement
    window.resize(900, 900);

    window.show();

    return app.exec();
}
