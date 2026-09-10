#pragma once

#include "token.hpp"

#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& source);

    std::vector<Token> tokenize();

private:
    const std::string& source;
    size_t position = 0;
    int line = 1;
    int column = 1;

    char peek() const;
    char advance();

    void skipWhitespace();
};
