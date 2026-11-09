#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/search_result_model.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QPlainTextEdit>
#include <QItemSelectionModel>


namespace aiws::gui {

SearchWidget::SearchWidget(QWidget* parent)
    : QWidget(parent) {

    query_edit_ = new QLineEdit(this);
    query_edit_->setPlaceholderText(
        "Search the current corpus");

    result_count_spin_ = new QSpinBox(this);
    result_count_spin_->setRange(1, 20);
    result_count_spin_->setValue(3);

    search_button_ = new QPushButton("Search", this);

    results_view_ = new QTableView(this);
    result_model_ = new SearchResultModel(this);

    results_view_->setModel(result_model_);

    selected_result_text_ =
        new QPlainTextEdit(this);
    selected_result_text_->setReadOnly(true);

    view_source_button_ =
        new QPushButton("View Source", this);

    // Query row
    QHBoxLayout* query_layout =
        new QHBoxLayout;

    query_layout->addWidget(
        new QLabel("Query:"));

    query_layout->addWidget(query_edit_);

    query_layout->addWidget(
        new QLabel("Results:"));

    query_layout->addWidget(
        result_count_spin_);

    query_layout->addWidget(
        search_button_);

    // Selected Result row
    QHBoxLayout* selected_layout =
        new QHBoxLayout;

    selected_layout->addWidget(
        new QLabel("Selected Result:"));

    selected_layout->addStretch();

    selected_layout->addWidget(
        view_source_button_);

    // Main layout
    QVBoxLayout* layout =
        new QVBoxLayout(this);

    layout->addLayout(query_layout);
    layout->addWidget(results_view_);
    layout->addLayout(selected_layout);
    layout->addWidget(selected_result_text_);

    connect(search_button_,
            &QPushButton::clicked,
            this,
            &SearchWidget::request_search);

    connect(query_edit_,
            &QLineEdit::textChanged,
            this,
            &SearchWidget::query_changed);

    connect(view_source_button_,
            &QPushButton::clicked,
            this,
            &SearchWidget::request_source_document);

    connect(results_view_->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &SearchWidget::show_selected_result);


    reset();
}

QString SearchWidget::query() const {
    return query_edit_->text();
}

int SearchWidget::result_count() const {
    return result_count_spin_->value();
}

bool SearchWidget::has_query() const {
    return !query_edit_->text().trimmed().isEmpty();
}

void SearchWidget::set_search_enabled(bool enabled) {
    search_button_->setEnabled(enabled);
}


void SearchWidget::set_results(
    std::vector<SearchResult> results) {

    result_model_->set_results(std::move(results));
}


void SearchWidget::clear_results() {

    result_model_->set_results({});

    selected_result_text_->clear();

    view_source_button_->setEnabled(false);
}

void SearchWidget::reset() {

    query_edit_->clear();

    result_count_spin_->setValue(3);   // adjust if spec says otherwise

    clear_results();

    search_button_->setEnabled(false);
}

void SearchWidget::request_search() {

    emit search_requested(
        query_edit_->text(),
        result_count_spin_->value());
}

void SearchWidget::show_selected_result() {
    QModelIndex index = results_view_->currentIndex();

    if (!index.isValid()) {

        selected_result_text_->clear();
        view_source_button_->setEnabled(false);

        emit selected_chunk_changed("");

        return;
    }

    const SearchResult* result =
        result_model_->result_at(index.row());

    if (result == nullptr) {
        return;
    }

    QString details;

    details += "Document: "
               + QString::fromStdString(result->document_id)
               + "\n";

    details += "Chunk: "
               + QString::fromStdString(result->chunk_id)
               + "\n";

    details += "Score: "
               + QString::number(result->score)
               + "\n";

    details += "Matched Terms: "
               + QString::number(result->matched_terms)
               + "\n\n";

    details += "Text:\n";
    details += QString::fromStdString(result->text);

    selected_result_text_->setPlainText(details);

    view_source_button_->setEnabled(true);

    emit selected_chunk_changed(
        QString::fromStdString(result->chunk_id));
}


void SearchWidget::request_source_document() {
    QModelIndex index =
        results_view_->currentIndex();

    if (!index.isValid()) {
        return;
    }

    SearchResult result =
        *result_model_->result_at(index.row());

    emit source_document_requested(
        QString::fromStdString(result.document_id));
}

}  // namespace aiws::gui
