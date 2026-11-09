#include "aiws/retrieval_engine.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace aiws {

double RetrievalEngine::canonical_score(double value) {
    constexpr double scale = 1'000'000'000'000.0;
    return std::round(value * scale) / scale;
}

std::vector<SearchResult> RetrievalEngine::search(const std::string& query, int k, const std::vector<Chunk>& chunks, const CorpusIndex& index) const {
    
    if (k < 0)
        throw std::invalid_argument("k must be non-negative");
    
    if (k == 0 || chunks.empty())
        return {};

    const auto raw_terms = TextProcessor::terms(query);
    if (raw_terms.empty())
        return {};

    std::vector<std::string> terms;
    std::unordered_set<std::string> seen;
    for (const auto& term : raw_terms) {
        if (seen.insert(term).second) terms.push_back(term);
    }

    struct Accum {
        double base{}; std::size_t matched{};
    };
    
    std::unordered_map<std::size_t, Accum> accum;
    const double n = static_cast<double>(chunks.size());

    for (const auto& term : terms) {
        const auto* postings = index.postings(term);
        if (!postings)
            continue;
        const double df = static_cast<double>(postings->size());
        const double idf = std::log((n + 1.0) / (df + 1.0)) + 1.0;
        for (const auto& posting : *postings) {
            const double tf = 1.0 + std::log(static_cast<double>(posting.frequency));
            auto& a = accum[posting.chunk_index];
            a.base += tf * idf;
            ++a.matched;
        }
    }

    std::vector<SearchResult> results;
    results.reserve(accum.size());
    for (const auto& [chunk_index, a] : accum) {
        const Chunk& c = chunks[chunk_index];
        const double coverage = 1.0 + 0.10 *
            (static_cast<double>(a.matched) / static_cast<double>(terms.size()));
        results.push_back(SearchResult{
            c.id, c.document_id, c.sequence, c.text,
            canonical_score(a.base * coverage), a.matched});
    }

    std::sort(results.begin(), results.end(), [&](const SearchResult& a, const SearchResult& b) {
        if (a.score != b.score)
            return a.score > b.score;
        
        const Chunk& ca = chunks.at(index.chunk_index(a.chunk_id));
        const Chunk& cb = chunks.at(index.chunk_index(b.chunk_id));
        
        if (ca.document_order != cb.document_order)
            return ca.document_order < cb.document_order;
        return ca.sequence < cb.sequence;
    });

    if (static_cast<std::size_t>(k) < results.size())
        results.resize(static_cast<std::size_t>(k));
    
    return results;
}

}  // namespace aiws
