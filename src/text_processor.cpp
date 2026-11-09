#include "aiws/text_processor.hpp"

#include <algorithm>

namespace aiws {
namespace {

bool is_ascii_alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

char lower_ascii(unsigned char c) {
    if (c >= 'A' && c <= 'Z')
        return static_cast<char>(c - 'A' + 'a');
    
    return static_cast<char>(c);
}

bool blank_line_between(const std::string& text, std::size_t a, std::size_t b) {
    bool first_newline = false;
    
    for (std::size_t i = a; i < b;) {
        const char ch = text[i];
        if (ch == '\r' || ch == '\n') {
            if (ch == '\r' && i + 1 < b && text[i + 1] == '\n') ++i;
            if (first_newline)
                return true;
            first_newline = true;
            ++i;
            continue;
        }
        if (first_newline && ch != ' ' && ch != '\t')
            first_newline = false;
        ++i;
    }
    return false;
}

}  // namespace

std::vector<TokenInfo> TextProcessor::tokenize(const std::string& text) {
    std::vector<TokenInfo> result;
    std::string current;
    std::size_t token_begin = 0;

    auto finish = [&](std::size_t end) {
        if (!current.empty()) {
            result.push_back(TokenInfo{current, token_begin, end, 0});
            current.clear();
        }
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (is_ascii_alnum(c)) {
            if (current.empty())
                token_begin = i;
            current.push_back(lower_ascii(c));
        } else {
            finish(i);
        }
    }
    finish(text.size());

    std::size_t paragraph = 0;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (i > 0 && blank_line_between(text, result[i - 1].end, result[i].begin)) {
            ++paragraph;
        }
        result[i].paragraph = paragraph;
    }
    return result;
}

std::vector<std::string> TextProcessor::terms(const std::string& text) {
    const auto tokens = tokenize(text);
    std::vector<std::string> result;
    result.reserve(tokens.size());
    for (const auto& token : tokens)
        result.push_back(token.token);
    return result;
}

std::string TextProcessor::normalize(const std::string& text) {
    const auto tokens = tokenize(text);
    return join(tokens, 0, tokens.size());
}

std::string TextProcessor::join(const std::vector<TokenInfo>& tokens,
                                std::size_t begin,
                                std::size_t end) {
    end = std::min(end, tokens.size());
    std::string result;
    for (std::size_t i = begin; i < end; ++i) {
        if (!result.empty())
            result.push_back(' ');
        result += tokens[i].token;
    }
    return result;
}

std::string TextProcessor::join(const std::vector<std::string>& tokens,
                                std::size_t begin,
                                std::size_t end) {
    end = std::min(end, tokens.size());
    std::string result;
    for (std::size_t i = begin; i < end; ++i) {
        if (!result.empty())
            result.push_back(' ');
        result += tokens[i];
    }
    return result;
}

}  // namespace aiws
