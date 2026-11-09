#pragma once

#include "aiws/retrieval_strategy.hpp"

namespace aiws {

class RetrievalEngine final : public RetrievalStrategy {
public:
    std::vector<SearchResult> search(const std::string& query,
                                     int k,
                                     const std::vector<Chunk>& chunks,
                                     const CorpusIndex& index) const override;

private:
    static double canonical_score(double value);
};

}  // namespace aiws
