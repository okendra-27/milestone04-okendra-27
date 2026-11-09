#pragma once

#include <cstddef>
#include <string>

namespace aiws {

struct Chunk {
    std::string id;
    std::string document_id;
    std::size_t document_order{};
    std::size_t sequence{};
    std::string text;
    std::size_t token_count{};
    std::size_t source_begin{};
    std::size_t source_end{};
};

struct SearchResult {
    std::string chunk_id;
    std::string document_id;
    std::size_t chunk_sequence{};
    std::string text;
    double score{};
    std::size_t matched_terms{};
};

struct ContextItem {
    std::string chunk_id;
    std::string document_id;
    std::size_t chunk_sequence{};
    std::string text;
    std::size_t token_count{};
    double score{};
    bool truncated{};
};

}  // namespace aiws
