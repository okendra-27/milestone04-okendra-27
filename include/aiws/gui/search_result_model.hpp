#pragma once

#include <QAbstractTableModel>

#include <vector>

#include "aiws/processing_types.hpp"

namespace aiws::gui {

/**
 * @brief Qt table model that presents SearchResult objects in the results view.
 *
 * The model exposes one row per search result and the M3 result columns used by
 * the QTableView. It also provides access to the SearchResult represented by a
 * selected row.
 */
class SearchResultModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    /**
     * @brief Constructs an empty search-result model.
     * @param parent Optional Qt parent object.
     */
    explicit SearchResultModel(QObject* parent = nullptr);

    /**
     * @brief Returns the number of search-result rows in the model.
     * @param parent Parent model index. The M3 model is a flat table.
     * @return Number of top-level result rows, or 0 for a valid parent index.
     */
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief Returns the number of columns in the M3 search-results table.
     * @param parent Parent model index. The M3 model is a flat table.
     * @return Number of top-level table columns, or 0 for a valid parent index.
     */
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief Returns display data for a search-result table cell.
     * @param index Model index identifying the requested cell.
     * @param role Qt item-data role being requested.
     * @return Display value for supported indexes and roles; otherwise an empty QVariant.
     */
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;

    /**
     * @brief Returns header data for the search-results table.
     * @param section Row or column header section.
     * @param orientation Header orientation.
     * @param role Qt item-data role being requested.
     * @return Header value for supported sections and roles; otherwise an empty QVariant.
     */
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    /**
     * @brief Replaces all search results represented by the model.
     * @param results New ranked search results.
     */
    void set_results(std::vector<SearchResult> results);

    /**
     * @brief Removes all search results from the model.
     */
    void clear();

    /**
     * @brief Returns the SearchResult represented by a model row.
     * @param row Zero-based result row.
     * @return Pointer to the corresponding result, or nullptr when row is invalid.
     */
    const SearchResult* result_at(int row) const noexcept;

private:
    std::vector<SearchResult> results_;
};

}  // namespace aiws::gui
