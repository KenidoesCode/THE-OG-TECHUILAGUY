#pragma once
#include <string>

enum class TokenKind {
    Fn, Return, Let,
    TypeI32,
    Identifier, Integer,
    Arrow,
    LParen, RParen,
    LBrace, RBrace,
    Colon, Semicolon,
    Equal, Plus,
    End
};

struct Token {
    TokenKind kind;
    std::string text;
    int line;
    int column;
};
