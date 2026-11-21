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
#include <exception>
#include <memory>
#include <utility>

namespace aiws::gui {

class CorpusState {
public:
    virtual ~CorpusState() = default;
    virtual void build(MainWindow& window) = 0;
    virtual void workspace_changed(MainWindow& window) = 0;
    virtual void reset(MainWindow& window) = 0;
    virtual void build_finished(MainWindow& window, int version, int generation,
                                ProcessingCore&& core) = 0;
    virtual void build_failed(MainWindow& window, int version, int generation) = 0;
    virtual bool has_current_corpus() const noexcept = 0;
    virtual bool build_running() const noexcept = 0;
};

class ReadyState;
class BuildingState;
class CurrentState;

class ReadyState final : public CorpusState {
public:
    void build(MainWindow& w) override {
        if (!w.workspace_.documents().empty()) w.request_build();
    }
    void workspace_changed(MainWindow& w) override { w.transition_to(std::make_unique<ReadyState>()); }
    void reset(MainWindow& w) override { w.transition_to(std::make_unique<ReadyState>()); }
    void build_finished(MainWindow& w, int, int, ProcessingCore&&) override {
        w.transition_to(std::make_unique<ReadyState>());
    }
    void build_failed(MainWindow& w, int, int) override { w.transition_to(std::make_unique<ReadyState>()); }
    bool has_current_corpus() const noexcept override { return false; }
    bool build_running() const noexcept override { return false; }
};

class BuildingState final : public CorpusState {
public:
    explicit BuildingState(bool current) : current_(current) {}
    void build(MainWindow&) override {}
    void workspace_changed(MainWindow& w) override {
        w.cancel_worker_build();
        w.transition_to(std::make_unique<ReadyState>());
    }
    void reset(MainWindow& w) override { workspace_changed(w); }
    void build_finished(MainWindow& w, int version, int generation, ProcessingCore&& core) override;
    void build_failed(MainWindow& w, int version, int generation) override;
    bool has_current_corpus() const noexcept override { return current_; }
    bool build_running() const noexcept override { return true; }
private:
    bool current_;
};

class CurrentState final : public CorpusState {
public:
    void build(MainWindow& w) override {
        if (!w.workspace_.documents().empty()) w.request_build();
    }
    void workspace_changed(MainWindow& w) override {
        w.cancel_worker_build();
        w.transition_to(std::make_unique<ReadyState>());
    }
    void reset(MainWindow& w) override { workspace_changed(w); }
    void build_finished(MainWindow& w, int version, int generation, ProcessingCore&& core) override {
        if (generation != w.build_generation_) return;
        if (version != w.workspace_version_) {
            w.transition_to(std::make_unique<ReadyState>());
            w.update_action_states();
            return;
        }
        w.processing_core_ = std::move(core);
        w.workspace_widget_->set_corpus_status(QString("Corpus: %1 chunks").arg(w.processing_core_.chunk_count()));
        w.status_label_->setText("Corpus built.");
        w.transition_to(std::make_unique<CurrentState>());
        w.update_action_states();
    }
    void build_failed(MainWindow& w, int version, int generation) override {
        if (generation != w.build_generation_) return;
        if (version != w.workspace_version_) w.transition_to(std::make_unique<ReadyState>());
        else {
            w.status_label_->setText("Build failed.");
            QTimer::singleShot(0, &w, &MainWindow::show_build_failure);
            w.transition_to(std::make_unique<CurrentState>());
        }
        w.update_action_states();
    }
    bool has_current_corpus() const noexcept override { return true; }
    bool build_running() const noexcept override { return false; }
};

void BuildingState::build_finished(MainWindow& w, int version, int generation, ProcessingCore&& core) {
    if (generation != w.build_generation_) return;
    if (version != w.workspace_version_) {
        w.transition_to(std::make_unique<ReadyState>());
        w.status_label_->setText("Build discarded (workspace changed)");
        w.update_action_states();
        return;
    }
    w.processing_core_ = std::move(core);
    w.workspace_widget_->set_corpus_status(QString("Corpus: %1 chunks").arg(w.processing_core_.chunk_count()));
    w.clear_processing_outputs();
    w.status_label_->setText("Corpus built.");
    w.transition_to(std::make_unique<CurrentState>());
    w.update_action_states();
}

void BuildingState::build_failed(MainWindow& w, int version, int generation) {
    if (generation != w.build_generation_) return;
    if (version != w.workspace_version_) {
        w.transition_to(std::make_unique<ReadyState>());
        w.update_action_states();
        return;
    }
    w.status_label_->setText("Build failed.");
    QTimer::singleShot(0, &w, &MainWindow::show_build_failure);
    w.transition_to(current_ ? std::unique_ptr<CorpusState>(new CurrentState())
                             : std::unique_ptr<CorpusState>(new ReadyState()));
    w.update_action_states();
}
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    corpus_state_ = std::make_unique<ReadyState>();

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
    build_thread_ = std::thread(&MainWindow::worker_loop, this);
}

//DECONSTRUCTOR
MainWindow::~MainWindow()
{
    ++build_generation_;
    if (ipc_server_) ipc_server_->stop();
    stop_worker();
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
    corpus_state_->build(*this);
}

void MainWindow::finish_build(
    int build_version,
    int generation,
    ProcessingCore&& new_core)
{
    corpus_state_->build_finished(*this, build_version, generation, std::move(new_core));
}

void MainWindow::build_failed(int build_version, int generation)
{
    corpus_state_->build_failed(*this, build_version, generation);
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

    corpus_state_->reset(*this);

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
        has_documents && !corpus_state_->build_running());

    bool can_search =
        corpus_state_->has_current_corpus() &&
        search_widget_->has_query();

    search_widget_->set_search_enabled(
        can_search);

    term_inspector_widget_->set_inspection_enabled(
        corpus_state_->has_current_corpus());

    context_widget_->set_build_context_enabled(
        can_search);
}

void MainWindow::invalidate_corpus() {

    if (corpus_state_->build_running()) status_label_->setText("Build cancelled (workspace changed)");
    corpus_state_->workspace_changed(*this);

    workspace_widget_->set_corpus_status(
        "Corpus not built");

    clear_processing_outputs();

    update_action_states();
}

void MainWindow::transition_to(std::unique_ptr<CorpusState> next_state) {
    corpus_state_ = std::move(next_state);
}

void MainWindow::request_build() {
    const bool keep_corpus = corpus_state_->has_current_corpus();
    const int generation = ++build_generation_;
    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    active_cancel_ = cancelled;
    auto request = std::make_unique<BuildRequest>(BuildRequest{workspace_, workspace_version_, generation, build_delay_ms_, cancelled});
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        pending_build_ = std::move(request);
    }
    status_label_->setText("Building corpus...");
    transition_to(std::make_unique<BuildingState>(keep_corpus));
    update_action_states();
    worker_condition_.notify_one();
}

void MainWindow::cancel_worker_build() {
    if (active_cancel_) active_cancel_->store(true);
    ++build_generation_;
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        pending_build_.reset();
    }
}

void MainWindow::stop_worker() {
    if (!build_thread_.joinable()) return;
    if (active_cancel_) active_cancel_->store(true);
#ifdef AIWS_FAULT_SILENT_SHUTDOWN
    const bool idle_at_shutdown = !corpus_state_->build_running();
#endif
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        worker_stopping_ = true;
        pending_build_.reset();
    }
#ifndef AIWS_FAULT_SILENT_SHUTDOWN
    worker_condition_.notify_all();
#else
    if (!idle_at_shutdown) {
        worker_condition_.notify_all();
    } else {
        std::unique_lock<std::mutex> lock(worker_mutex_);
        worker_condition_.wait(lock, [] { return false; });
    }
#endif
    build_thread_.join();
}

void MainWindow::worker_loop() {
    while (true) {
        std::unique_ptr<BuildRequest> request;
        {
            std::unique_lock<std::mutex> lock(worker_mutex_);
            worker_condition_.wait(lock, [this] { return worker_stopping_ || pending_build_ != nullptr; });
            if (worker_stopping_) return;
            request = std::move(pending_build_);
        }
        try {
            ProcessingCore core;
            core.set_build_delay_ms(0);
            Workspace prefix;
#ifdef AIWS_FAULT_NO_CANCEL
            const bool ignore_cancellation = true;
#else
            const bool ignore_cancellation = false;
#endif
            for (const auto& document : request->workspace.documents()) {
                if (!ignore_cancellation && request->cancelled->load()) break;
                if (request->delay_ms > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(request->delay_ms));
                if (!ignore_cancellation && request->cancelled->load()) break;
                prefix.add_document(document);
                core.rebuild(prefix);
            }
            if (!ignore_cancellation && request->cancelled->load()) {
#ifdef AIWS_FAULT_NO_CANCEL
                QMetaObject::invokeMethod(this, [this, version=request->version, generation=request->generation] {
                    build_failed(version, generation);
                }, Qt::QueuedConnection);
#endif
                continue;
            }
            auto result = std::make_shared<ProcessingCore>(std::move(core));
            QMetaObject::invokeMethod(this, [this, version=request->version, generation=request->generation, result]() mutable {
                finish_build(version, generation, std::move(*result));
            }, Qt::QueuedConnection);
        } catch (...) {
            if (request->cancelled->load()) {
#ifdef AIWS_FAULT_NO_CANCEL
                QMetaObject::invokeMethod(this, [this, version=request->version, generation=request->generation] {
                    build_failed(version, generation);
                }, Qt::QueuedConnection);
#endif
                continue;
            }
            QMetaObject::invokeMethod(this, [this, version=request->version, generation=request->generation] { build_failed(version, generation); }, Qt::QueuedConnection);
        }
    }
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
