#include "lexer.hpp"

#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source)
    : source(source) {}

char Lexer::peek() const {
    if (position >= source.size())
        return '\0';

    return source[position];
}

static char peekAt(const std::string& source, size_t position) {
    if (position >= source.size())
        return '\0';

    return source[position];
}

char Lexer::advance() {
    char c = peek();

    if (c == '\0')
        return c;

    ++position;

    if (c == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }

    return c;
}

void Lexer::skipWhitespace() {
    while (true) {
        while (std::isspace(
            static_cast<unsigned char>(peek()))) {
            advance();
        }

        if (peek() == '/' && peekAt(source, position + 1) == '/') {
            while (peek() != '\0' && peek() != '\n') {
                advance();
            }
            continue;
        }

        break;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (peek() != '\0') {
        skipWhitespace();

        if (peek() == '\0')
            break;

        int tokenLine = line;
        int tokenColumn = column;

        char c = peek();

        if (std::isalpha(
                static_cast<unsigned char>(c)) ||
            c == '_') {

            std::string text;

            while (
                std::isalnum(
                    static_cast<unsigned char>(peek())) ||
                peek() == '_'
            ) {
                text += advance();
            }

            TokenKind kind;

            if (text == "fn")
                kind = TokenKind::Fn;
            else if (text == "return")
                kind = TokenKind::Return;
            else if (text == "let")
                kind = TokenKind::Let;
            else if (text == "if")
                kind = TokenKind::If;
            else if (text == "else")
                kind = TokenKind::Else;
            else if (text == "while")
                kind = TokenKind::While;
            else if (text == "i32")
                kind = TokenKind::TypeI32;
            else
                kind = TokenKind::Identifier;

            tokens.push_back({
                kind,
                text,
                tokenLine,
                tokenColumn
            });

            continue;
        }

        if (std::isdigit(
                static_cast<unsigned char>(c))) {

            std::string text;

            while (std::isdigit(
                static_cast<unsigned char>(peek()))) {
                text += advance();
            }

            tokens.push_back({
                TokenKind::Integer,
                text,
                tokenLine,
                tokenColumn
            });

            continue;
        }

        std::string text;
        TokenKind kind;

        switch (c) {

            case '(':
                text = "(";
                kind = TokenKind::LParen;
                advance();
                break;

            case ')':
                text = ")";
                kind = TokenKind::RParen;
                advance();
                break;

            case '{':
                text = "{";
                kind = TokenKind::LBrace;
                advance();
                break;

            case '}':
                text = "}";
                kind = TokenKind::RBrace;
                advance();
                break;

            case ':':
                text = ":";
                kind = TokenKind::Colon;
                advance();
                break;

            case ';':
                text = ";";
                kind = TokenKind::Semicolon;
                advance();
                break;

            case ',':
                text = ",";
                kind = TokenKind::Comma;
                advance();
                break;

            case '+':
                text = "+";
                kind = TokenKind::Plus;
                advance();
                break;

            case '*':
                text = "*";
                kind = TokenKind::Star;
                advance();
                break;

            case '/':
                text = "/";
                kind = TokenKind::Slash;
                advance();
                break;

            case '-':
                advance();

                if (peek() == '>') {
                    advance();
                    text = "->";
                    kind = TokenKind::Arrow;
                } else {
                    text = "-";
                    kind = TokenKind::Minus;
                }

                break;

            case '=':
                advance();

                if (peek() == '=') {
                    advance();
                    text = "==";
                    kind = TokenKind::EqualEqual;
                } else {
                    text = "=";
                    kind = TokenKind::Equal;
                }

                break;

            case '!':
                advance();

                if (peek() == '=') {
                    advance();
                    text = "!=";
                    kind = TokenKind::NotEqual;
                } else {
                    throw std::runtime_error(
                        "Unexpected '!'; expected !="
                    );
                }

                break;

            case '>':
                advance();

                if (peek() == '=') {
                    advance();
                    text = ">=";
                    kind = TokenKind::GreaterEqual;
                } else {
                    text = ">";
                    kind = TokenKind::Greater;
                }

                break;

            case '<':
                advance();

                if (peek() == '=') {
                    advance();
                    text = "<=";
                    kind = TokenKind::LessEqual;
                } else {
                    text = "<";
                    kind = TokenKind::Less;
                }

                break;

            default:
                throw std::runtime_error(
                    "Unexpected character: " +
                    std::string(1, c)
                );
        }

        tokens.push_back({
            kind,
            text,
            tokenLine,
            tokenColumn
        });
    }

    tokens.push_back({
        TokenKind::End,
        "",
        line,
        column
    });

    return tokens;
}
