#pragma once

#include "aiws/processing_types.hpp"

#include <cstddef>
#include <vector>

namespace aiws {

class ContextStrategy {
public:
    virtual ~ContextStrategy() = default;
    virtual std::vector<ContextItem> build(const std::vector<SearchResult>& ranked,
                                           std::size_t token_budget) const = 0;
};

}  // namespace aiws
