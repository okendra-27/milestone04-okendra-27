#pragma once

#include <QMainWindow>
#include <QString>

#include "aiws/processing_core.hpp"
#include "aiws/workspace.hpp"
#include "aiws/ipc_server.hpp"

//For GUI Threads
#include <thread>
#include <atomic>

class QLabel;
class QPushButton;
class QMessageBox;
class QString;
class QWidget;

namespace aiws::gui {

class ContextWidget;
class SearchWidget;
class TermInspectorWidget;
class WorkspaceWidget;

/**
 * @brief Main application window and coordinator for the M3 AI Workspace GUI.
 *
 * MainWindow owns the application Workspace and ProcessingCore, composes the
 * major GUI components, and coordinates application-wide state changes and
 * backend operations in response to component events.
 */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Function-pointer type used for the application Exit action.
     *
     * The callback receives the main window as a parent for any exit-related
     * interaction. In M3, the registered callback asks the user to confirm the
     * exit. Declining the confirmation leaves the running application and its
     * current state unchanged.
     */
    using ExitCallback = void (*)(QWidget* parent);

    /**
     * @brief Constructs the main AI Workspace application window.
     * @param parent Optional Qt parent widget.
     */
    explicit MainWindow(QWidget* parent = nullptr);

    //UPDATED DECONSTRUCTOR
    ~MainWindow() override;

    /**
     * @brief Registers the callback invoked when the user requests Exit.
     * @param callback Function pointer to invoke for the Exit action. A null
     * callback means that an Exit request has no registered callback to invoke.
     */
    void set_exit_callback(ExitCallback callback) noexcept;

    // ---- M4 required public API (see specification, Section 5) ----------

    /**
     * @brief Adds the file at path exactly as the Add Document action does.
     * @return false, changing nothing, if the file cannot be read.
     */
    bool add_document_file(const QString& path);

    /**
     * @brief Sets the extra build time per document for builds started after this call.
     * @param ms_per_document Delay in milliseconds (the --build-delay-ms value).
     */
    void set_build_delay_ms(int ms_per_document);
    void start_ipc_server(const QString& socket_path = QString());
    QString build_status() const;

    // Everything below this line is the M3 design. In M4 it is yours to
    // change: add, remove, or redesign private members, slots, and helpers.

private slots:
    /** @brief Handles a request to select and add a document to the workspace. */
    void add_document();

    /** @brief Rebuilds the processing corpus from the current non-empty workspace. */
    void build_corpus();

    /**
     * @brief Updates the selected-document display for the requested workspace row.
     * @param row Zero-based document row, or a negative value when no row is selected.
     */
    void document_selected(int row);

    /**
     * @brief Performs a search of the current corpus and presents the returned results.
     * @param query Query text to search for.
     * @param k Maximum number of results requested.
     */
    void search_requested(const QString& query, int k);

    /**
     * @brief Responds to a query edit by updating application action availability.
     * @param query Updated query text.
     */
    void query_changed(const QString& query);

    /**
     * @brief Builds and displays context for the current query and search settings.
     * @param token_budget Maximum token budget requested for the context.
     */
    void build_context_requested(int token_budget);
    /**
     * @brief Selects and displays the workspace document identified by a search result.
     * @param document_id Identifier of the source document to reveal.
     */
    void source_document_requested(const QString& document_id);

    /**
     * @brief Queries and presents term statistics for the current corpus.
     * @param term Term to inspect.
     * @param chunk_id Selected result chunk identifier; may be empty, in which
     * case document frequency is still available but chunk frequency is not.
     */
    void term_inspection_requested(const QString& term, const QString& chunk_id);

    /**
     * @brief Restores the application and its major GUI components to initial state.
     */
    void reset_application();

    /**
     * @brief Invokes the registered Exit callback for the current exit request.
     */
    void exit_application();

private:
    /**
     * @brief Updates availability of actions that depend on current application state.
     */
    void update_action_states();

    /**
     * @brief Marks the processing corpus as no longer current for the workspace.
     *
     * Processing output that depends on the previous corpus is no longer made
     * available after invalidation.
     */
    void invalidate_corpus();

    /**
     * @brief Clears displayed search and context output that depends on processing state.
     */
    void clear_processing_outputs();

    /**
     * @brief Uses std::thread and std::atomic to create a workspace version and run the build
     */

    std::thread build_thread_;

    std::atomic<int> workspace_version_{0};

    std::atomic<bool> build_running_{false};

    int build_delay_ms_{0};
    int build_generation_{0};

    void finish_build(
        int build_version,
        int generation,
        ProcessingCore&& new_core);

    void build_failed(int build_version, int generation);
    void show_build_failure();

    Workspace workspace_;
    ProcessingCore processing_core_;
    bool corpus_current_{false};
    ExitCallback exit_callback_{nullptr};

    WorkspaceWidget* workspace_widget_{nullptr};
    SearchWidget* search_widget_{nullptr};
    TermInspectorWidget* term_inspector_widget_{nullptr};
    ContextWidget* context_widget_{nullptr};
    QLabel* status_label_{nullptr};
    QPushButton* reset_button_{nullptr};
    QPushButton* exit_button_{nullptr};
    QMessageBox* build_failure_message_{nullptr};
    std::unique_ptr<aiws::IpcServer> ipc_server_;
};

}  // namespace aiws::gui
