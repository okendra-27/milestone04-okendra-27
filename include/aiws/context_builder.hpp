#pragma once

#include "aiws/context_strategy.hpp"

namespace aiws {

class ContextBuilder final : public ContextStrategy {
public:
    std::vector<ContextItem> build(const std::vector<SearchResult>& ranked,
                                   std::size_t token_budget) const override;
};

}  // namespace aiws
