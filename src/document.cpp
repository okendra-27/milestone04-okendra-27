#include "aiws/document.hpp"

#include <utility>

namespace aiws {

Document::Document(std::string id, std::string title, std::string text)
    : id_(std::move(id)), title_(std::move(title)), text_(std::move(text)) {}

const std::string& Document::id() const noexcept { return id_; }
const std::string& Document::title() const noexcept { return title_; }
const std::string& Document::text() const noexcept { return text_; }

}  // namespace aiws
