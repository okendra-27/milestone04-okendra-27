#include "aiws/gui/main_window.hpp"
#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <thread>
#include <atomic>
#include <QMetaObject>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <exception>

namespace aiws::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    workspace_widget_ = new WorkspaceWidget(this);
    search_widget_ = new SearchWidget(this);
    term_inspector_widget_ = new TermInspectorWidget(this);
    context_widget_ = new ContextWidget(this);

    status_label_ = new QLabel("Current status: Ready", this);

    reset_button_ = new QPushButton("Reset", this);
    exit_button_ = new QPushButton("Exit", this);

    // Central widget
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    // Main layout
    QVBoxLayout* main_layout = new QVBoxLayout;

    main_layout->addWidget(workspace_widget_);
    main_layout->addWidget(search_widget_);

    // Bottom row containing term inspector and context
    QHBoxLayout* lower_layout = new QHBoxLayout;
    lower_layout->addWidget(term_inspector_widget_);
    lower_layout->addWidget(context_widget_);

    main_layout->addLayout(lower_layout);

    // Status row
    QHBoxLayout* status_layout = new QHBoxLayout;

    status_layout->addWidget(status_label_);
    status_layout->addStretch();
    status_layout->addWidget(reset_button_);
    status_layout->addWidget(exit_button_);

    main_layout->addLayout(status_layout);

    central->setLayout(main_layout);

    connect(reset_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::reset_application);

    connect(exit_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::exit_application);

    connect(workspace_widget_,
            &WorkspaceWidget::add_document_requested,
            this,
            &MainWindow::add_document);

    connect(workspace_widget_,
            &WorkspaceWidget::build_corpus_requested,
            this,
            &MainWindow::build_corpus);

    connect(workspace_widget_,
            &WorkspaceWidget::document_selected,
            this,
            &MainWindow::document_selected);

    connect(search_widget_,
            &SearchWidget::search_requested,
            this,
            &MainWindow::search_requested);

    connect(search_widget_,
            &SearchWidget::query_changed,
            this,
            &MainWindow::query_changed);

    connect(search_widget_,
            &SearchWidget::source_document_requested,
            this,
            &MainWindow::source_document_requested);

    connect(search_widget_,
            &SearchWidget::selected_chunk_changed,
            term_inspector_widget_,
            &TermInspectorWidget::set_selected_chunk);

    connect(search_widget_,
            &SearchWidget::selected_chunk_changed,
            term_inspector_widget_,
            &TermInspectorWidget::set_selected_chunk);

    connect(term_inspector_widget_,
            &TermInspectorWidget::term_inspection_requested,
            this,
            &MainWindow::term_inspection_requested);

    connect(context_widget_,
            &ContextWidget::build_context_requested,
            this,
            &MainWindow::build_context_requested);


    update_action_states();
}

//DECONSTRUCTOR
MainWindow::~MainWindow()
{
    ++build_generation_;
    if (ipc_server_) ipc_server_->stop();
    if (build_thread_.joinable()) {
        build_thread_.join();
    }
}

void MainWindow::start_ipc_server(const QString& socket_path) {
#ifdef _WIN32
    if (!socket_path.isEmpty())
        throw std::runtime_error("--ipc-socket is supported only on POSIX systems");
    return;
#else
    if (ipc_server_) return;
    const std::string path = socket_path.toUtf8().toStdString();
    ipc_server_ = std::make_unique<aiws::IpcServer>(path,
        [this](const std::string& utf8_path) {
            const QString file_path = QString::fromUtf8(utf8_path.data(),
                                                       static_cast<qsizetype>(utf8_path.size()));
            QMetaObject::invokeMethod(this, [this, file_path]() {
                add_document_file(file_path);
            }, Qt::QueuedConnection);
        });
    ipc_server_->start();
#endif
}

void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    exit_callback_ = callback;
}



void MainWindow::add_document() {
    QString filename =
        QFileDialog::getOpenFileName(
            this,
            "Open Document",
            QString(),
            "Text Files (*.txt)");

    if (filename.isEmpty()) {
        return;
    }

    add_document_file(filename);
}

bool MainWindow::add_document_file(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString text = QString::fromUtf8(file.readAll());
    Document doc(filename.toStdString(), QFileInfo(filename).fileName().toStdString(),
                 text.toStdString());
    workspace_.add_document(doc);
    ++workspace_version_;
    workspace_widget_->add_document(QString::fromStdString(doc.title()));
    const int row = static_cast<int>(workspace_.documents().size() - 1);
    workspace_widget_->select_document(row);
    workspace_widget_->show_selected_document(QString::fromStdString(doc.title()),
                                               QString::fromStdString(doc.text()));
    invalidate_corpus();
    update_action_states();
    return true;
}

//REBUILD
void MainWindow::build_corpus()
{
    if (build_running_) {
        return;
    }

    build_running_ = true;

    status_label_->setText("Building corpus...");
    update_action_states();

    Workspace snapshot = workspace_;

    const int build_version = workspace_version_.load();
    const int generation = ++build_generation_;
    const int delay_ms = build_delay_ms_;

    if (build_thread_.joinable()) {
        // A prior worker can finish just before its queued GUI callback runs.
        // It has already completed by the time a new build is enabled.
        build_thread_.join();
    }

    build_thread_ = std::thread(
        [this, snapshot, build_version, generation, delay_ms]()
        {
            try {
                ProcessingCore temp_core;
                temp_core.set_build_delay_ms(delay_ms);

                temp_core.rebuild(snapshot);

                auto result = std::make_shared<ProcessingCore>(std::move(temp_core));
                QMetaObject::invokeMethod(this,
                    [this, build_version, generation, result]() mutable {
                        finish_build(build_version, generation, std::move(*result));
                    }, Qt::QueuedConnection);
            }
            catch (...) {
#ifndef AIWS_FAULT_LOSE_FAILURE
                QMetaObject::invokeMethod(
                    this,
                    [this, build_version, generation]() { build_failed(build_version, generation); },
                    Qt::QueuedConnection);
#endif
            }
        });
}

void MainWindow::finish_build(
    int build_version,
    int generation,
    ProcessingCore&& new_core)
{
    if (generation != build_generation_) return;
    build_running_ = false;

#ifndef AIWS_FAULT_ACCEPT_STALE
    if (build_version != workspace_version_) {
        status_label_->setText(workspace_.documents().empty()
                                   ? "Ready"
                                   : "Build discarded (workspace changed)");
        update_action_states();
        return;
    }
#else
    (void)build_version;
#endif

    processing_core_ = std::move(new_core);

    corpus_current_ = true;

    workspace_widget_->set_corpus_status(
        QString("Corpus: %1 chunks")
            .arg(processing_core_.chunk_count()));

    clear_processing_outputs();

    status_label_->setText("Corpus built.");

    update_action_states();
}

void MainWindow::build_failed(int build_version, int generation)
{
#ifdef AIWS_FAULT_LOSE_FAILURE
    (void)generation;
    return;
#endif
    if (generation != build_generation_) return;
    build_running_ = false;

    if (build_version != workspace_version_) {
        status_label_->setText(workspace_.documents().empty()
                                   ? "Ready"
                                   : "Build discarded (workspace changed)");
        update_action_states();
        return;
    }

    status_label_->setText("Build failed.");
    QTimer::singleShot(0, this, &MainWindow::show_build_failure);

    update_action_states();
}

void MainWindow::show_build_failure() {
    if (!build_failure_message_) {
        build_failure_message_ = new QMessageBox(QMessageBox::Warning,
            "Corpus build failed",
            "The corpus could not be built. The workspace is still available; correct the input and try again.",
            QMessageBox::Ok, this);
        build_failure_message_->setModal(false);
    }
    build_failure_message_->show();
    build_failure_message_->raise();
}

void MainWindow::document_selected(int row) {
    if (row < 0) {
        workspace_widget_->clear_selected_document();
        return;
    }

    const auto& docs = workspace_.documents();

    if (static_cast<size_t>(row) >= docs.size()) {
        return;
    }

    const auto& doc = docs[row];

    workspace_widget_->show_selected_document(
        QString::fromStdString(doc.title()),
        QString::fromStdString(doc.text()));
}

void MainWindow::search_requested(const QString& query, int k) {
    auto results =
        processing_core_.search(query.toStdString(), k);

    search_widget_->set_results(std::move(results));
    (void)query;  // To suppress unused-parameter warning.
    (void)k;  // To suppress unused-parameter warning.
}

void MainWindow::query_changed(const QString&) {
    update_action_states();
}

void MainWindow::build_context_requested(int token_budget) {
    auto results =
        processing_core_.build_context(
            search_widget_->query().toStdString(),
            search_widget_->result_count(),
            static_cast<std::size_t>(token_budget));

    context_widget_->set_context(std::move(results));
}

void MainWindow::source_document_requested(const QString& document_id) {

    const auto& docs = workspace_.documents();

    for (std::size_t i = 0; i < docs.size(); ++i) {

        if (QString::fromStdString(docs[i].id())
            == document_id) {

            workspace_widget_->select_document(
                static_cast<int>(i));

            workspace_widget_->show_selected_document(
                QString::fromStdString(docs[i].title()),
                QString::fromStdString(docs[i].text()));

            break;
        }
    }
}


void MainWindow::term_inspection_requested(
    const QString& term,
    const QString& chunk_id) {

    std::size_t df =
        processing_core_.document_frequency(
            term.toStdString());

    if (chunk_id.isEmpty()) {

        term_inspector_widget_->set_statistics(
            df,
            false,
            0);

        return;
    }

    std::size_t tf =
        processing_core_.term_frequency(
            term.toStdString(),
            chunk_id.toStdString());

    term_inspector_widget_->set_statistics(
        df,
        true,
        tf);
}

//REBUILD
void MainWindow::reset_application() {
    workspace_.clear();

    //Every workspace change gets a new version
    workspace_version_++;

    corpus_current_ = false;

    workspace_widget_->reset();
    search_widget_->reset();
    term_inspector_widget_->reset();
    context_widget_->reset();

    status_label_->setText("Ready");

    update_action_states();
}

//REBUILD
void MainWindow::exit_application() {

    if (exit_callback_ != nullptr) {
        exit_callback_(this);
    }
}

void MainWindow::update_action_states() {

    bool has_documents =
        !workspace_.documents().empty();

    workspace_widget_->set_build_corpus_enabled(
        has_documents && !build_running_);

    bool can_search =
        corpus_current_ &&
        search_widget_->has_query();

    search_widget_->set_search_enabled(
        can_search);

    term_inspector_widget_->set_inspection_enabled(
        corpus_current_);

    context_widget_->set_build_context_enabled(
        can_search);
}

void MainWindow::invalidate_corpus() {

    if (build_running_) status_label_->setText("Building corpus for previous workspace...");

    corpus_current_ = false;

    workspace_widget_->set_corpus_status(
        "Corpus not built");

    clear_processing_outputs();

    update_action_states();
}

void MainWindow::set_build_delay_ms(int ms_per_document) {
    build_delay_ms_ = ms_per_document < 0 ? 0 : ms_per_document;
}

QString MainWindow::build_status() const {
    return status_label_->text();
}

void MainWindow::clear_processing_outputs() {

    search_widget_->clear_results();

    term_inspector_widget_->clear_statistics();

    context_widget_->clear_context();
}

}  // namespace aiws::gui
