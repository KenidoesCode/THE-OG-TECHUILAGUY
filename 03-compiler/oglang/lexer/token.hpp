#pragma once

#include <cstddef>
#include <string>

enum class TokenType {
    Fn,
    Return,
    Let,
    TypeI32,

    Identifier,
    Integer,

    Arrow,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,

    Colon,
    Semicolon,
    Equals,
    Plus,

    EndOfFile,
    Invalid
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t position;
};
