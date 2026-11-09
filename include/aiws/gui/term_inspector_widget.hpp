#pragma once

#include <QWidget>
#include <QString>

#include <cstddef>

#include "aiws/gui/resettable.hpp"

class QLabel;
class QLineEdit;
class QPushButton;

namespace aiws::gui {

/**
 * @brief Presents term-frequency inspection controls and statistics.
 *
 * The component collects a term from the user and displays document frequency
 * for the current corpus. When a search-result chunk is selected, it can also
 * display that term's frequency in the selected chunk. ProcessingCore queries
 * are coordinated by MainWindow.
 */
class TermInspectorWidget final : public QWidget, public Resettable {
    Q_OBJECT

public:
    /** @brief Constructs the term-inspector GUI component. @param parent Optional Qt parent widget. */
    explicit TermInspectorWidget(QWidget* parent = nullptr);

    /** @brief Sets whether the Inspect action is available. @param enabled true to enable it. */
    void set_inspection_enabled(bool enabled);

    /**
     * @brief Records the chunk associated with the current search-result selection.
     * @param chunk_id Selected chunk identifier, or an empty string when no result is selected.
     */
    void set_selected_chunk(const QString& chunk_id);

    /**
     * @brief Displays term statistics returned by the processing backend.
     * @param document_frequency Number of corpus documents containing the term.
     * @param has_chunk_frequency Whether a selected chunk frequency is available.
     * @param chunk_frequency Frequency of the term in the selected chunk when available.
     */
    void set_statistics(std::size_t document_frequency,
                        bool has_chunk_frequency,
                        std::size_t chunk_frequency);

    /** @brief Clears all displayed term statistics. */
    void clear_statistics();

    /**
     * @brief Restores the component to its initial application state.
     *
     * The term and selected chunk are cleared, statistics return to their
     * neutral display, and Inspect is disabled.
     */
    void reset() override;

signals:
    /**
     * @brief Emitted when the user requests term inspection.
     * @param term Term entered by the user.
     * @param chunk_id Current selected chunk identifier; may be empty.
     */
    void term_inspection_requested(const QString& term, const QString& chunk_id);

private slots:
    /** @brief Converts an Inspect button event into the term_inspection_requested signal. */
    void request_inspection();

private:
    QLineEdit* term_edit_{nullptr};
    QPushButton* inspect_button_{nullptr};
    QLabel* document_frequency_value_{nullptr};
    QLabel* chunk_frequency_value_{nullptr};
    QString selected_chunk_id_;
};

}  // namespace aiws::gui
