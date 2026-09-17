#pragma once

#include <string>

enum class TokenKind {
    Fn,
    Return,
    Let,
    Struct,
    Enum,

    TypeI32,
    TypePtr,
    TypeConstPtr,

    Identifier,
    Integer,

    Arrow,

    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,

    Colon,
    Semicolon,
    Comma,
    Equal,
    Dot,

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
