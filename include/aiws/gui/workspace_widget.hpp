#pragma once

#include <QWidget>
#include <QString>

#include "aiws/gui/resettable.hpp"

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QString;

namespace aiws::gui {

/**
 * @brief Presents the workspace and selected-document portions of the M3 GUI.
 *
 * The widget displays document titles, the currently selected document, corpus
 * status, and the actions used to add documents and build the corpus. Backend
 * workspace and processing operations are coordinated by MainWindow.
 */
class WorkspaceWidget final : public QWidget, public Resettable {
    Q_OBJECT

public:
    /**
     * @brief Constructs the workspace and selected-document GUI component.
     * @param parent Optional Qt parent widget.
     */
    explicit WorkspaceWidget(QWidget* parent = nullptr);

    /**
     * @brief Adds a document title to the workspace list.
     * @param title Title to display for the document.
     */
    void add_document(const QString& title);

    /**
     * @brief Selects the requested document row in the workspace list.
     * @param row Zero-based document row to select.
     *
     * This operation changes GUI selection only; it does not perform backend
     * processing.
     */
    void select_document(int row);

    /**
     * @brief Displays the selected document's title and complete text.
     * @param title Document title to display.
     * @param text Document text to display.
     */
    void show_selected_document(const QString& title, const QString& text);

    /**
     * @brief Clears the selected-document display.
     */
    void clear_selected_document();

    /**
     * @brief Updates the displayed corpus status.
     * @param text Corpus-status text to display.
     */
    void set_corpus_status(const QString& text);
    QString corpus_status() const;

    /**
     * @brief Sets whether the Build Corpus action is available.
     * @param enabled true to enable the action; false to disable it.
     */
    void set_build_corpus_enabled(bool enabled);

    /**
     * @brief Restores the workspace component to its initial application state.
     *
     * The document list and selected-document display are cleared, the corpus
     * status returns to its initial value, and Build Corpus is unavailable.
     */
    void reset() override;

signals:
    /** @brief Emitted when the user requests that a document be added. */
    void add_document_requested();

    /** @brief Emitted when the user requests that the corpus be built. */
    void build_corpus_requested();

    /**
     * @brief Emitted when the current document selection changes.
     * @param row Zero-based selected row, or a negative value when no row is selected.
     */
    void document_selected(int row);

private:
    QListWidget* document_list_{nullptr};
    QPushButton* add_document_button_{nullptr};
    QPushButton* build_corpus_button_{nullptr};
    QLabel* corpus_status_label_{nullptr};
    QLabel* document_title_label_{nullptr};
    QPlainTextEdit* document_text_{nullptr};
};

}  // namespace aiws::gui
