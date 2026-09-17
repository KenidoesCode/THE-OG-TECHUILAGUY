#pragma once

#include <string>

enum class TokenKind {
    Fn,
    Return,
    Let,

    TypeI32,

    Identifier,
    Integer,

    Arrow,

    LParen,
    RParen,
    LBrace,
    RBrace,

    Colon,
    Semicolon,
    Comma,
    Equal,

    Plus,
    Minus,
    Star,
    Slash,

    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    EqualEqual,
    NotEqual,

    If,
    Else,

    End
};

struct Token {
    TokenKind kind;
    std::string text;
    int line;
    int column;
};
