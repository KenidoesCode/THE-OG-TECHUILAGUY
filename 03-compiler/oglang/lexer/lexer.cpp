#include "lexer.hpp"
#include <cctype>

Lexer::Lexer(const std::string& source)
    : source(source) {}

char Lexer::peek() const {
    if (position >= source.size())
        return '\0';

    return source[position];
}

char Lexer::advance() {
    return source[position++];
}

void Lexer::skipWhitespace() {
    while (std::isspace(static_cast<unsigned char>(peek())))
        advance();
}

Token Lexer::identifier() {
    std::size_t start = position;

    while (std::isalnum(static_cast<unsigned char>(peek())) ||
           peek() == '_')
        advance();

    std::string text = source.substr(start, position - start);

    if (text == "fn")
        return {TokenType::Fn, text, start};

    if (text == "return")
        return {TokenType::Return, text, start};

    if (text == "let")
        return {TokenType::Let, text, start};

    if (text == "i32")
        return {TokenType::TypeI32, text, start};

    return {TokenType::Identifier, text, start};
}

Token Lexer::integer() {
    std::size_t start = position;

    while (std::isdigit(static_cast<unsigned char>(peek())))
        advance();

    return {
        TokenType::Integer,
        source.substr(start, position - start),
        start
    };
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (position < source.size()) {
        skipWhitespace();

        if (position >= source.size())
            break;

        std::size_t start = position;
        char c = peek();

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(identifier());
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(integer());
            continue;
        }

        advance();

        switch (c) {
            case '(':
                tokens.push_back({TokenType::LeftParen, "(", start});
                break;
            case ')':
                tokens.push_back({TokenType::RightParen, ")", start});
                break;
            case '{':
                tokens.push_back({TokenType::LeftBrace, "{", start});
                break;
            case '}':
                tokens.push_back({TokenType::RightBrace, "}", start});
                break;
            case ':':
                tokens.push_back({TokenType::Colon, ":", start});
                break;
            case ';':
                tokens.push_back({TokenType::Semicolon, ";", start});
                break;

            case '=':
                tokens.push_back({TokenType::Equals, "=", start});
                break;

            case '+':
                tokens.push_back({TokenType::Plus, "+", start});
                break;
            case '-':
                if (peek() == '>') {
                    advance();
                    tokens.push_back({TokenType::Arrow, "->", start});
                } else {
                    tokens.push_back({TokenType::Invalid, "-", start});
                }
                break;
            default:
                tokens.push_back({
                    TokenType::Invalid,
                    std::string(1, c),
                    start
                });
        }
    }

    tokens.push_back({TokenType::EndOfFile, "", position});
    return tokens;
}
