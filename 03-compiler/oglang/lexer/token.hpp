#pragma once

#include <string>

enum class TokenKind {
    Fn,
    Return,
    Let,

    TypeI32,
    TypePtr,

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
    Ampersand,

    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    EqualEqual,
    NotEqual,

    If,
    Else,
    While,

    End
};

struct Token {
    TokenKind kind;
    std::string text;
    int line;
    int column;
};
