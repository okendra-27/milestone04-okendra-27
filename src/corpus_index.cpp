#include "aiws/corpus_index.hpp"

#include "aiws/text_processor.hpp"

#include <stdexcept>
#include <unordered_map>

namespace aiws {

CorpusIndex::CorpusIndex(const std::vector<Chunk>& chunks) { build(chunks);
}

void CorpusIndex::build(const std::vector<Chunk>& chunks) {
    std::unordered_map<std::string, std::vector<Posting>> next_postings;
    std::unordered_map<std::string, std::size_t> next_lookup;

    for (std::size_t i = 0; i < chunks.size(); ++i) {
        if (!next_lookup.emplace(chunks[i].id, i).second) {
            throw std::invalid_argument("duplicate chunk id: " + chunks[i].id);
        }
        std::unordered_map<std::string, std::size_t> frequency;
        for (const auto& term : TextProcessor::terms(chunks[i].text)) ++frequency[term];
        
        for (const auto& [term, count] : frequency) {
            next_postings[term].push_back(Posting{i, count});
        }
    }

    postings_ = std::move(next_postings);
    chunk_by_id_ = std::move(next_lookup);
}

std::size_t CorpusIndex::document_frequency(const std::string& normalized_term) const noexcept {
    const auto it = postings_.find(normalized_term);
    return it == postings_.end() ? 0 : it->second.size();
}

std::size_t CorpusIndex::term_frequency(const std::string& normalized_term, const std::string& chunk_id) const noexcept {
    const auto lookup = chunk_by_id_.find(chunk_id);
    
    if (lookup == chunk_by_id_.end()) return 0;
    
    const auto it = postings_.find(normalized_term);
    
    if (it == postings_.end()) return 0;
    
    for (const auto& posting : it->second) {
        if (posting.chunk_index == lookup->second) return posting.frequency;
    }
    return 0;
}

const std::vector<CorpusIndex::Posting>* CorpusIndex::postings(
    const std::string& normalized_term) const noexcept {
    const auto it = postings_.find(normalized_term);
    return it == postings_.end() ? nullptr : &it->second;
}

const Chunk* CorpusIndex::find_chunk(const std::vector<Chunk>& chunks,
                                     const std::string& chunk_id) const noexcept {
    const auto it = chunk_by_id_.find(chunk_id);
    if (it == chunk_by_id_.end() || it->second >= chunks.size()) return nullptr;
    return &chunks[it->second];
}

std::size_t CorpusIndex::chunk_index(const std::string& chunk_id) const {
    const auto it = chunk_by_id_.find(chunk_id);
    if (it == chunk_by_id_.end()) throw std::out_of_range("unknown chunk id");
    return it->second;
}

}  // namespace aiws
