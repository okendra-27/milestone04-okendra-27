#pragma once

#include "aiws/document.hpp"

#include <vector>

namespace aiws {

class Workspace {
public:
    void add_document(Document document);
    const std::vector<Document>& documents() const noexcept;
    void clear() noexcept;

private:
    std::vector<Document> documents_;
};

}  // namespace aiws
