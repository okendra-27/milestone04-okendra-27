#pragma once

#include <string>

namespace aiws {

class Document {
public:
    Document() = default;
    Document(std::string id, std::string title, std::string text);

    const std::string& id() const noexcept;
    const std::string& title() const noexcept;
    const std::string& text() const noexcept;

private:
    std::string id_;
    std::string title_;
    std::string text_;
};

}  // namespace aiws
