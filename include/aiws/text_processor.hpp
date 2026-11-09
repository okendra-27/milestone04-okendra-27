#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace aiws {

struct TokenInfo {
    std::string token;
    std::size_t begin{};
    std::size_t end{};
    std::size_t paragraph{};
};

class TextProcessor {
public:
    static std::vector<TokenInfo> tokenize(const std::string& text);
    static std::vector<std::string> terms(const std::string& text);
    static std::string normalize(const std::string& text);
    static std::string join(const std::vector<TokenInfo>& tokens,
                            std::size_t begin,
                            std::size_t end);
    static std::string join(const std::vector<std::string>& tokens,
                            std::size_t begin,
                            std::size_t end);
};

}  // namespace aiws
