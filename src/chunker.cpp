#include "aiws/chunker.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <stdexcept>

namespace aiws {

Chunker::Chunker(ChunkingPolicy policy) : policy_(policy) {
    if (policy_.max_tokens == 0 || policy_.overlap >= policy_.max_tokens ||
        policy_.paragraph_window > policy_.max_tokens) {
        throw std::invalid_argument("invalid chunking policy");
    }
}

std::vector<Chunk> Chunker::chunk(const Document& document,
                                  std::size_t document_order) const {
    const auto tokens = TextProcessor::tokenize(document.text());
    std::vector<Chunk> chunks;
    std::size_t start = 0;
    std::size_t sequence = 0;

    while (start < tokens.size()) {
        std::size_t end = tokens.size();
        if (tokens.size() - start > policy_.max_tokens) {
            const std::size_t hard_end = start + policy_.max_tokens;
            const std::size_t earliest = hard_end - policy_.paragraph_window;
            std::size_t preferred = hard_end;
            bool found = false;
            for (std::size_t boundary = earliest; boundary <= hard_end; ++boundary) {
                if (boundary > start && boundary < tokens.size() &&
                    tokens[boundary - 1].paragraph != tokens[boundary].paragraph) {
                    preferred = boundary;
                    found = true;
                }
            }
            end = found ? preferred : hard_end;
        }

        Chunk c;
        c.document_id = document.id();
        c.document_order = document_order;
        c.sequence = sequence;
        c.id = document.id() + "#" + std::to_string(sequence);
        c.text = TextProcessor::join(tokens, start, end);
        c.token_count = end - start;
        c.source_begin = tokens[start].begin;
        c.source_end = tokens[end - 1].end;
        chunks.push_back(std::move(c));

        if (end == tokens.size()) break;
        start = std::max(start + 1, end - policy_.overlap);
        ++sequence;
    }
    return chunks;
}

}  // namespace aiws
