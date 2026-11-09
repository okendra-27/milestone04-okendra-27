#pragma once

#include <QWidget>

#include <vector>

#include "aiws/gui/resettable.hpp"
#include "aiws/processing_types.hpp"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSlider;

namespace aiws::gui {

/**
 * @brief Presents controls and output for building query context.
 *
 * ContextWidget owns the token-budget control, Build Context action, and
 * read-only context display. It reports user requests to MainWindow; backend
 * context construction is not performed by this component.
 */
class ContextWidget final : public QWidget, public Resettable {
    Q_OBJECT

public:
    /** @brief Constructs the context GUI component. @param parent Optional Qt parent widget. */
    explicit ContextWidget(QWidget* parent = nullptr);

    /** @brief Returns the token budget currently selected by the user. */
    int token_budget() const;

    /** @brief Sets whether Build Context is available. @param enabled true to enable it. */
    void set_build_context_enabled(bool enabled);

    /**
     * @brief Replaces the context display with the supplied context items.
     * @param items Context items returned by ProcessingCore::build_context().
     */
    void set_context(const std::vector<ContextItem>& items);

    /** @brief Clears all currently displayed context output. */
    void clear_context();

    /**
     * @brief Restores the component to its initial application state.
     *
     * The token budget returns to its initial value, displayed context is
     * cleared, and Build Context is disabled.
     */
    void reset() override;

signals:
    /**
     * @brief Emitted when the user requests context construction.
     * @param token_budget Current token budget selected by the user.
     */
    void build_context_requested(int token_budget);

private slots:
    /** @brief Converts a Build Context button event into the component request signal. */
    void request_context();

    /** @brief Updates the visible budget value when the slider changes. */
    void budget_changed(int value);

private:
    QSlider* context_budget_slider_{nullptr};
    QLabel* budget_value_label_{nullptr};
    QPushButton* build_context_button_{nullptr};
    QPlainTextEdit* context_text_{nullptr};
};

}  // namespace aiws::gui
