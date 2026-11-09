#pragma once

#include <QWidget>

#include <vector>

#include "aiws/gui/resettable.hpp"
#include "aiws/processing_types.hpp"

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QString;

namespace aiws::gui {

class SearchResultModel;

/**
 * @brief Presents query input, ranked search results, and selected-result details.
 *
 * SearchWidget owns the query controls and results view. It uses
 * SearchResultModel to present SearchResult objects and reports search, query,
 * selection, and View Source events to the application coordinator.
 */
class SearchWidget final : public QWidget, public Resettable {
    Q_OBJECT

public:
    /** @brief Constructs the search GUI component. @param parent Optional Qt parent widget. */
    explicit SearchWidget(QWidget* parent = nullptr);

    /** @brief Returns the current query text exactly as entered in the query field. */
    QString query() const;

    /** @brief Returns the currently requested maximum number of search results. */
    int result_count() const;

    /** @brief Returns true when the current query contains non-whitespace text. */
    bool has_query() const;

    /** @brief Sets whether the Search action is available. @param enabled true to enable it. */
    void set_search_enabled(bool enabled);

    /** @brief Replaces the model contents with new ranked results. */
    void set_results(std::vector<SearchResult> results);

    /**
     * @brief Clears search results and result-dependent presentation state.
     *
     * The table contents, current result selection, selected-result details,
     * and View Source availability are cleared.
     */
    void clear_results();

    /**
     * @brief Restores the component to its initial application state.
     *
     * Query text and results are cleared, the result-count control returns to
     * its initial value, and Search is disabled.
     */
    void reset() override;

signals:
    /** @brief Emitted when the user requests a search. */
    void search_requested(const QString& query, int k);

    /** @brief Emitted whenever the query text changes. */
    void query_changed(const QString& query);

    /**
     * @brief Emitted when View Source is requested for the selected result.
     * @param document_id Document identifier stored in the selected SearchResult.
     */
    void source_document_requested(const QString& document_id);

    /**
     * @brief Emitted when the selected search-result chunk changes.
     * @param chunk_id Selected chunk identifier, or an empty string when no result is selected.
     */
    void selected_chunk_changed(const QString& chunk_id);

private slots:
    /** @brief Converts a Search button event into the search_requested signal. */
    void request_search();

    /** @brief Updates selected-result details after the table selection changes. */
    void show_selected_result();

    /** @brief Emits a source-document request for the currently selected result. */
    void request_source_document();

private:
    QLineEdit* query_edit_{nullptr};
    QSpinBox* result_count_spin_{nullptr};
    QPushButton* search_button_{nullptr};
    QTableView* results_view_{nullptr};
    SearchResultModel* result_model_{nullptr};
    QPlainTextEdit* selected_result_text_{nullptr};
    QPushButton* view_source_button_{nullptr};
};

}  // namespace aiws::gui
