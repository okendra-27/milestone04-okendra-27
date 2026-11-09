#pragma once

#include "aiws/chunking_strategy.hpp"

#include <cstddef>

namespace aiws {

struct ChunkingPolicy {
    std::size_t max_tokens{120};
    std::size_t overlap{20};
    std::size_t paragraph_window{20};
};

class Chunker final : public ChunkingStrategy {
public:
    explicit Chunker(ChunkingPolicy policy = {});
    std::vector<Chunk> chunk(const Document& document,
                             std::size_t document_order) const override;

private:
    ChunkingPolicy policy_;
};

}  // namespace aiws
