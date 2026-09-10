#pragma once

#include "token.hpp"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();

private:
    std::string source;
    std::size_t position = 0;

    char peek() const;
    char advance();
    void skipWhitespace();

    Token identifier();
    Token integer();
};
