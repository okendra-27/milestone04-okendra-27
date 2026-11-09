#include "aiws/gui/workspace_widget.hpp"
#include <qboxlayout.h>

#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QPlainTextEdit>

namespace aiws::gui {

WorkspaceWidget::WorkspaceWidget(QWidget* parent)
    : QWidget(parent) {

    document_list_ = new QListWidget(this);

    add_document_button_ =
        new QPushButton("Add Document", this);

    build_corpus_button_ =
        new QPushButton("Build Corpus", this);

    corpus_status_label_ =
        new QLabel("Corpus: Not built", this);

    document_title_label_ =
        new QLabel("Selected Document -", this);

    document_text_ =
        new QPlainTextEdit(this);

    document_text_->setReadOnly(true);

    // ----- Left side -----

    QVBoxLayout* workspace_layout =
        new QVBoxLayout;

    QHBoxLayout* workspace_header =
        new QHBoxLayout;

    workspace_header->addWidget(
        new QLabel("Workspace"));

    workspace_header->addWidget(
        add_document_button_);

    workspace_header->addWidget(
        build_corpus_button_);

    workspace_header->addStretch();

    workspace_layout->addLayout(
        workspace_header);

    workspace_layout->addWidget(
        document_list_);

    workspace_layout->addWidget(
        corpus_status_label_);

    // ----- Right side -----

    QVBoxLayout* selected_layout =
        new QVBoxLayout;

    selected_layout->addWidget(
        document_title_label_);

    selected_layout->addWidget(
        document_text_);

    // ----- Main -----

    QHBoxLayout* main_layout =
        new QHBoxLayout(this);

    main_layout->addLayout(workspace_layout);
    main_layout->addLayout(selected_layout);

    // Events
    connect(add_document_button_,
            &QPushButton::clicked,
            this,
            &WorkspaceWidget::add_document_requested);

    connect(build_corpus_button_,
            &QPushButton::clicked,
            this,
            &WorkspaceWidget::build_corpus_requested);

    connect(document_list_,
            &QListWidget::currentRowChanged,
            this,
            &WorkspaceWidget::document_selected);

    reset();
}

void WorkspaceWidget::add_document(
    const QString& title) {

    document_list_->addItem(title);
}

void WorkspaceWidget::select_document(
    int row) {

    document_list_->setCurrentRow(row);
}

void WorkspaceWidget::show_selected_document(
    const QString& title,
    const QString& text) {

    document_title_label_->setText(
        "Selected Document - " + title);

    document_text_->setPlainText(text);
}

void WorkspaceWidget::clear_selected_document() {

    document_title_label_->setText(
        "Selected Document -");

    document_text_->clear();
}

void WorkspaceWidget::set_corpus_status(
    const QString& text) {

    corpus_status_label_->setText(text);
}

QString WorkspaceWidget::corpus_status() const {
    return corpus_status_label_->text();
}

void WorkspaceWidget::set_build_corpus_enabled(
    bool enabled) {

    build_corpus_button_->setEnabled(enabled);
}

void WorkspaceWidget::reset() {

    document_list_->clear();

    clear_selected_document();

    corpus_status_label_->setText(
        "Corpus: Not built");

    build_corpus_button_->setEnabled(false);
}

}  // namespace aiws::gui
