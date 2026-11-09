#include "aiws/gui/context_widget.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QPlainTextEdit>

namespace aiws::gui {

ContextWidget::ContextWidget(QWidget* parent)
    : QWidget(parent) {

    context_budget_slider_ =
        new QSlider(Qt::Horizontal, this);

    context_budget_slider_->setRange(50, 1000);
    context_budget_slider_->setSingleStep(50);
    context_budget_slider_->setPageStep(50);
    context_budget_slider_->setValue(300);

    budget_value_label_ =
        new QLabel("300", this);

    build_context_button_ =
        new QPushButton("Build Context", this);

    context_text_ =
        new QPlainTextEdit(this);

    context_text_->setReadOnly(true);

    QLabel* title =
        new QLabel("Context:", this);

    QHBoxLayout* controls =
        new QHBoxLayout;

    controls->addWidget(
        new QLabel("Token budget:"));

    controls->addWidget(
        context_budget_slider_);

    controls->addWidget(
        budget_value_label_);

    controls->addWidget(
        build_context_button_);

    QVBoxLayout* main_layout =
        new QVBoxLayout(this);

    main_layout->addWidget(title);
    main_layout->addLayout(controls);
    main_layout->addWidget(context_text_);

    connect(build_context_button_,
            &QPushButton::clicked,
            this,
            &ContextWidget::request_context);

    connect(context_budget_slider_,
            &QSlider::valueChanged,
            this,
            &ContextWidget::budget_changed);

    reset();
}

int ContextWidget::token_budget() const {
    return context_budget_slider_->value();
}

void ContextWidget::set_build_context_enabled(
    bool enabled) {

    build_context_button_->setEnabled(enabled);
}

void ContextWidget::set_context(
    const std::vector<ContextItem>& items) {

    QString output;

    for (const auto& item : items) {

        output += "Document: "
                  + QString::fromStdString(item.document_id)
                  + "\n";

        output += "Chunk: "
                  + QString::fromStdString(item.chunk_id)
                  + "\n";

        output += "Tokens: "
                  + QString::number(item.token_count)
                  + "\n";

        output += "Score: "
                  + QString::number(item.score)
                  + "\n";

        output += "Truncated: "
                  + QString(item.truncated ? "true" : "false")
                  + "\n";

        output += "Text:\n";
        output += QString::fromStdString(item.text);
        output += "\n\n";
    }

    context_text_->setPlainText(output);
}

void ContextWidget::clear_context() {
    context_text_->clear();
}


void ContextWidget::reset() {

    context_budget_slider_->setValue(300);

    budget_value_label_->setText("300");

    clear_context();

    build_context_button_->setEnabled(false);
}


void ContextWidget::request_context() {

    emit build_context_requested(
        context_budget_slider_->value());
}

void ContextWidget::budget_changed(int value) {

    budget_value_label_->setText(
        QString::number(value));
}

}  // namespace aiws::gui
