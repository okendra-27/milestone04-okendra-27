#include "aiws/workspace.hpp"

#include <utility>

namespace aiws {

void Workspace::add_document(Document document) {
    documents_.push_back(std::move(document));
}

const std::vector<Document>& Workspace::documents() const noexcept {
    return documents_;
}

void Workspace::clear() noexcept { documents_.clear(); }

}  // namespace aiws
