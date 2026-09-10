#include "parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens) {}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::advance() {
    return tokens[current++];
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

const Token& Parser::expect(TokenType type) {
    if (!check(type))
        throw std::runtime_error(
            "Unexpected token: " + peek().lexeme
        );

    return advance();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (check(TokenType::Integer)) {
        auto token = advance();

        return std::make_unique<IntegerExpr>(
            std::stoll(token.lexeme)
        );
    }

    if (check(TokenType::Identifier)) {
        auto token = advance();

        return std::make_unique<VariableExpr>(
            token.lexeme
        );
    }

    throw std::runtime_error(
        "Expected expression, got: " + peek().lexeme
    );
}

std::unique_ptr<Expr> Parser::parseExpression() {
    auto left = parsePrimary();

    while (check(TokenType::Plus)) {
        advance();

        auto right = parsePrimary();

        left = std::make_unique<BinaryExpr>(
            '+',
            std::move(left),
            std::move(right)
        );
    }

    return left;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (check(TokenType::Let)) {
        advance();

        const Token& name = expect(TokenType::Identifier);

        expect(TokenType::Colon);
        expect(TokenType::TypeI32);
        expect(TokenType::Equals);

        auto initializer = parseExpression();

        expect(TokenType::Semicolon);

        return std::make_unique<LetStmt>(
            name.lexeme,
            "i32",
            std::move(initializer)
        );
    }

    if (check(TokenType::Return)) {
        advance();

        auto value = parseExpression();

        expect(TokenType::Semicolon);

        return std::make_unique<ReturnStmt>(
            std::move(value)
        );
    }

    throw std::runtime_error(
        "Unknown statement: " + peek().lexeme
    );
}

Function Parser::parseFunction() {
    expect(TokenType::Fn);

    const Token& name = expect(TokenType::Identifier);

    expect(TokenType::LeftParen);
    expect(TokenType::RightParen);

    expect(TokenType::Arrow);
    expect(TokenType::TypeI32);

    expect(TokenType::LeftBrace);

    Function function;
    function.name = name.lexeme;
    function.returnType = "i32";

    while (!check(TokenType::RightBrace)) {
        function.body.push_back(parseStatement());
    }

    expect(TokenType::RightBrace);
    expect(TokenType::EndOfFile);

    return function;
}
