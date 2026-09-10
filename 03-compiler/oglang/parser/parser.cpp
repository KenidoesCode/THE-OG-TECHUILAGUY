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

bool Parser::match(TokenKind kind) {
    if (peek().kind == kind) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenKind kind) {
    if (peek().kind != kind)
        throw std::runtime_error("Unexpected token: " + peek().text);

    return advance();
}

Function Parser::parseFunction() {
    expect(TokenKind::Fn);

    std::string name = expect(TokenKind::Identifier).text;

    expect(TokenKind::LParen);
    expect(TokenKind::RParen);

    expect(TokenKind::Arrow);

    std::string returnType;

    if (match(TokenKind::TypeI32))
        returnType = "i32";
    else
        throw std::runtime_error("Expected return type");

    expect(TokenKind::LBrace);

    Function function{name, returnType, {}};

    while (peek().kind != TokenKind::RBrace &&
           peek().kind != TokenKind::End) {
        function.body.push_back(parseStatement());
    }

    expect(TokenKind::RBrace);

    return function;
}

std::unique_ptr<Statement> Parser::parseStatement() {
    if (match(TokenKind::Let)) {
        std::string name = expect(TokenKind::Identifier).text;

        expect(TokenKind::Colon);

        std::string type;

        if (match(TokenKind::TypeI32))
            type = "i32";
        else
            throw std::runtime_error("Expected variable type");

        expect(TokenKind::Equal);

        auto initializer = parseExpression();

        expect(TokenKind::Semicolon);

        return std::make_unique<LetStmt>(
            name, type, std::move(initializer)
        );
    }

    if (match(TokenKind::Return)) {
        auto value = parseExpression();

        expect(TokenKind::Semicolon);

        return std::make_unique<ReturnStmt>(std::move(value));
    }

    throw std::runtime_error("Unknown statement");
}

std::unique_ptr<Expr> Parser::parseExpression() {
    auto left = parsePrimary();

    while (match(TokenKind::Plus)) {
        auto right = parsePrimary();

        left = std::make_unique<BinaryExpr>(
            '+',
            std::move(left),
            std::move(right)
        );
    }

    return left;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (peek().kind == TokenKind::Integer) {
        int value = std::stoi(advance().text);
        return std::make_unique<IntegerExpr>(value);
    }

    if (peek().kind == TokenKind::Identifier) {
        std::string name = advance().text;
        return std::make_unique<VariableExpr>(name);
    }

    throw std::runtime_error("Expected expression");
}
