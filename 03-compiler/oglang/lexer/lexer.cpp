#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source) : source(source) {}

char Lexer::peek() const {
    if (pos >= source.size()) return '\0';
    return source[pos];
}

char Lexer::advance() {
    char c = peek();
    if (c == '\0') return c;

    pos++;

    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }

    return c;
}

void Lexer::skipWhitespace() {
    while (std::isspace(static_cast<unsigned char>(peek())))
        advance();
}

Token Lexer::identifierOrKeyword() {
    int startLine = line;
    int startColumn = column;

    std::string text;

    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
        text += advance();

    if (text == "fn")     return {TokenKind::Fn, text, startLine, startColumn};
    if (text == "return") return {TokenKind::Return, text, startLine, startColumn};
    if (text == "let")   return {TokenKind::Let, text, startLine, startColumn};
    if (text == "i32")   return {TokenKind::TypeI32, text, startLine, startColumn};

    return {TokenKind::Identifier, text, startLine, startColumn};
}

Token Lexer::integer() {
    int startLine = line;
    int startColumn = column;

    std::string text;

    while (std::isdigit(static_cast<unsigned char>(peek())))
        text += advance();

    return {TokenKind::Integer, text, startLine, startColumn};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();

        int l = line;
        int c = column;
        char ch = peek();

        if (ch == '\0') {
            tokens.push_back({TokenKind::End, "", l, c});
            break;
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            tokens.push_back(identifierOrKeyword());
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            tokens.push_back(integer());
            continue;
        }

        if (ch == '-' && pos + 1 < source.size() && source[pos + 1] == '>') {
            advance();
            advance();
            tokens.push_back({TokenKind::Arrow, "->", l, c});
            continue;
        }

        TokenKind kind;

        switch (ch) {
            case '(': kind = TokenKind::LParen; break;
            case ')': kind = TokenKind::RParen; break;
            case '{': kind = TokenKind::LBrace; break;
            case '}': kind = TokenKind::RBrace; break;
            case ':': kind = TokenKind::Colon; break;
            case ';': kind = TokenKind::Semicolon; break;
            case '=': kind = TokenKind::Equal; break;
            case '+': kind = TokenKind::Plus; break;
            default:
                throw std::runtime_error(
                    "Invalid character at line " +
                    std::to_string(line) +
                    ", column " +
                    std::to_string(column)
                );
        }

        std::string text(1, advance());
        tokens.push_back({kind, text, l, c});
    }

    return tokens;
}
