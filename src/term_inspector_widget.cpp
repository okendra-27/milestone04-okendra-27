#include "aiws/gui/term_inspector_widget.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace aiws::gui {

TermInspectorWidget::TermInspectorWidget(QWidget* parent)
    : QWidget(parent) {

    term_edit_ = new QLineEdit(this);
    term_edit_->setPlaceholderText("Enter a term");

    inspect_button_ =
        new QPushButton("Inspect", this);

    document_frequency_value_ =
        new QLabel("—", this);

    chunk_frequency_value_ =
        new QLabel("—", this);

    QVBoxLayout* main_layout =
        new QVBoxLayout(this);

    main_layout->addWidget(
        new QLabel("Term Inspector:", this));

    QHBoxLayout* term_row =
        new QHBoxLayout;

    term_row->addWidget(
        new QLabel("Term:", this));

    term_row->addWidget(term_edit_);

    term_row->addWidget(inspect_button_);

    main_layout->addLayout(term_row);

    QHBoxLayout* stats_row =
        new QHBoxLayout;

    stats_row->addWidget(
        new QLabel("Document Frequency:", this));

    stats_row->addWidget(
        document_frequency_value_);

    stats_row->addStretch();

    stats_row->addWidget(
        new QLabel("Selected Chunk TF:", this));

    stats_row->addWidget(
        chunk_frequency_value_);

    main_layout->addLayout(stats_row);

    connect(inspect_button_,
            &QPushButton::clicked,
            this,
            &TermInspectorWidget::request_inspection);

    reset();
}

void TermInspectorWidget::set_inspection_enabled(
    bool enabled) {

    inspect_button_->setEnabled(enabled);
}

void TermInspectorWidget::set_selected_chunk(
    const QString& chunk_id) {

    selected_chunk_id_ = chunk_id;
}

void TermInspectorWidget::set_statistics(
    std::size_t document_frequency,
    bool has_chunk_frequency,
    std::size_t chunk_frequency) {

    document_frequency_value_->setText(
        QString::number(document_frequency));

    if (has_chunk_frequency) {

        chunk_frequency_value_->setText(
            QString::number(chunk_frequency));
    }
    else {

        chunk_frequency_value_->setText("—");
    }
}

void TermInspectorWidget::clear_statistics() {

    document_frequency_value_->setText("—");

    chunk_frequency_value_->setText("—");
}

void TermInspectorWidget::reset() {

    term_edit_->clear();

    selected_chunk_id_.clear();

    clear_statistics();

    inspect_button_->setEnabled(false);
}


void TermInspectorWidget::request_inspection() {

    QString term =
        term_edit_->text().trimmed();

    if (term.isEmpty()) {
        return;
    }

    emit term_inspection_requested(
        term,
        selected_chunk_id_);
}

}  // namespace aiws::gui
