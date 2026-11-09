#include "aiws/gui/search_result_model.hpp"

namespace aiws::gui {

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent) {
    // TODO
}

int SearchResultModel::rowCount(const QModelIndex& parent) const {

    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(results_.size());
}

int SearchResultModel::columnCount(const QModelIndex& parent) const {

    if (parent.isValid()) {
        return 0;
    }

    return 5;
}

QVariant SearchResultModel::data(const QModelIndex& index,int role) const {

    if (!index.isValid()) {
        return {};
    }

    if (role != Qt::DisplayRole) {
        return {};
    }

    const SearchResult& result =
        results_[index.row()];

    switch (index.column()) {

    case 0:
        return index.row() + 1;                 // Rank

    case 1:
        return QString::fromStdString(
            result.document_id);

    case 2:
        return QString::fromStdString(
            result.chunk_id);

    case 3:
        return result.score;

    case 4:
        return QString::fromStdString(
            result.text);
    }

    return {};
}

QVariant SearchResultModel::headerData(int section,Qt::Orientation orientation,int role) const {

    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation != Qt::Horizontal) {
        return {};
    }

    switch (section) {

    case 0:
        return "Rank";

    case 1:
        return "Document";

    case 2:
        return "Chunk";

    case 3:
        return "Score";

    case 4:
        return "Text";
    }

    return {};
}

void SearchResultModel::set_results(std::vector<SearchResult> results) {

    beginResetModel();

    results_ = std::move(results);

    endResetModel();
}

void SearchResultModel::clear() {

    beginResetModel();

    results_.clear();

    endResetModel();
}

const SearchResult*SearchResultModel::result_at(int row) const noexcept {

    if (row < 0 ||
        row >= static_cast<int>(results_.size())) {
        return nullptr;
    }

    return &results_[row];
}

}  // namespace aiws::gui
