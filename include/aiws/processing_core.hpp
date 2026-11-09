#pragma once

#include "aiws/chunking_strategy.hpp"
#include "aiws/context_strategy.hpp"
#include "aiws/processing_types.hpp"
#include "aiws/retrieval_strategy.hpp"
#include "aiws/workspace.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace aiws {

class ProcessingCore {
public:
    static constexpr std::size_t kMaxChunkTokens = 120;
    static constexpr std::size_t kChunkOverlap = 20;
    static constexpr std::size_t kParagraphPreferenceWindow = 20;

    ProcessingCore();
    ProcessingCore(std::unique_ptr<ChunkingStrategy> chunking,
                   std::unique_ptr<RetrievalStrategy> retrieval,
                   std::unique_ptr<ContextStrategy> context);
    ~ProcessingCore();

    ProcessingCore(ProcessingCore&&) noexcept;
    ProcessingCore& operator=(ProcessingCore&&) noexcept;
    ProcessingCore(const ProcessingCore&) = delete;
    ProcessingCore& operator=(const ProcessingCore&) = delete;

    static std::string normalize(const std::string& text);

    void rebuild(const Workspace& workspace);
    const std::vector<Chunk>& chunks() const noexcept;
    std::size_t chunk_count() const noexcept;

    std::size_t document_frequency(const std::string& term) const;
    std::size_t term_frequency(const std::string& term,
                               const std::string& chunk_id) const;

    std::vector<SearchResult> search(const std::string& query, int k) const;
    std::vector<ContextItem> build_context(const std::string& query,
                                           int k,
                                           std::size_t token_budget) const;
    void set_build_delay_ms(int ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int build_delay_ms_{0};

};

}  // namespace aiws
