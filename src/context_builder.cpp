#include "aiws/context_builder.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <unordered_set>

namespace aiws {

std::vector<ContextItem> ContextBuilder::build(const std::vector<SearchResult>& ranked, std::size_t token_budget) const {
    std::vector<ContextItem> context;
    std::size_t remaining = token_budget;
    std::unordered_set<std::string> used;

    for (const auto& result : ranked) {
        if (remaining == 0) break;
        if (!used.insert(result.chunk_id).second) continue;

        const auto terms = TextProcessor::terms(result.text);
        const std::size_t take = std::min(remaining, terms.size());
        const bool truncated = take < terms.size();
        context.push_back(ContextItem{result.chunk_id, result.document_id, result.chunk_sequence, TextProcessor::join(terms, 0, take), take, result.score, truncated});
        remaining -= take;
        if (truncated) break;
    }
    return context;
}

}  // namespace aiws
