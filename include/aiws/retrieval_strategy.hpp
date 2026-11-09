#pragma once

#include "aiws/corpus_index.hpp"
#include "aiws/processing_types.hpp"

#include <string>
#include <vector>

namespace aiws {

class RetrievalStrategy {
public:
    virtual ~RetrievalStrategy() = default;
    virtual std::vector<SearchResult> search(const std::string& query,
                                             int k,
                                             const std::vector<Chunk>& chunks,
                                             const CorpusIndex& index) const = 0;
};

}  // namespace aiws
