#include "parser.hpp"

#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens) {}

const Token& Parser::peek() const {
    return tokens[current];
}

const Token& Parser::peekNext() const {
    // The token stream always ends with an End token, so this never
    // runs past the end as long as we're not already past it.
    if (current + 1 >= tokens.size()) {
        return tokens.back();
    }

    return tokens[current + 1];
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
    if (peek().kind != kind) {
        throw std::runtime_error(
            "Unexpected token: " + peek().text
        );
    }

    return advance();
}

Program Parser::parseProgram() {
    Program program;

    while (peek().kind != TokenKind::End) {
        program.push_back(parseFunction());
    }

    return program;
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;

    expect(TokenKind::LParen);

    if (peek().kind != TokenKind::RParen) {
        while (true) {
            std::string name =
                expect(TokenKind::Identifier).text;

            expect(TokenKind::Colon);

            std::string type;

            if (match(TokenKind::TypeI32))
                type = "i32";
            else
                throw std::runtime_error(
                    "Expected parameter type"
                );

            params.push_back({name, type});

            if (match(TokenKind::Comma))
                continue;

            break;
        }
    }

    expect(TokenKind::RParen);

    return params;
}

std::vector<std::unique_ptr<Expr>> Parser::parseArgList() {
    std::vector<std::unique_ptr<Expr>> args;

    expect(TokenKind::LParen);

    if (peek().kind != TokenKind::RParen) {
        while (true) {
            args.push_back(parseExpression());

            if (match(TokenKind::Comma))
                continue;

            break;
        }
    }

    expect(TokenKind::RParen);

    return args;
}

Function Parser::parseFunction() {
    expect(TokenKind::Fn);

    std::string name =
        expect(TokenKind::Identifier).text;

    std::vector<Param> params = parseParamList();

    expect(TokenKind::Arrow);

    std::string returnType;

    if (match(TokenKind::TypeI32))
        returnType = "i32";
    else
        throw std::runtime_error(
            "Expected return type"
        );

    expect(TokenKind::LBrace);

    Function function{name, std::move(params), returnType, {}};

    while (peek().kind != TokenKind::RBrace &&
           peek().kind != TokenKind::End) {

        function.body.push_back(
            parseStatement()
        );
    }

    expect(TokenKind::RBrace);

    return function;
}

std::unique_ptr<Statement> Parser::parseStatement() {

    if (match(TokenKind::Let)) {

        std::string name =
            expect(TokenKind::Identifier).text;

        expect(TokenKind::Colon);

        std::string type;

        if (match(TokenKind::TypeI32))
            type = "i32";
        else
            throw std::runtime_error(
                "Expected variable type"
            );

        expect(TokenKind::Equal);

        auto initializer =
            parseExpression();

        expect(TokenKind::Semicolon);

        return std::make_unique<LetStmt>(
            name,
            type,
            std::move(initializer)
        );
    }

    if (match(TokenKind::Return)) {

        auto value =
            parseExpression();

        expect(TokenKind::Semicolon);

        return std::make_unique<ReturnStmt>(
            std::move(value)
        );
    }

    if (peek().kind == TokenKind::If)
        return parseIf();

    if (peek().kind == TokenKind::While)
        return parseWhile();

    if (peek().kind == TokenKind::Identifier &&
        peekNext().kind == TokenKind::Equal)
        return parseAssign();

    throw std::runtime_error(
        "Unknown statement: " + peek().text
    );
}

std::unique_ptr<Statement> Parser::parseAssign() {
    std::string name = expect(TokenKind::Identifier).text;

    expect(TokenKind::Equal);

    auto value = parseExpression();

    expect(TokenKind::Semicolon);

    return std::make_unique<AssignStmt>(
        name,
        std::move(value)
    );
}

std::unique_ptr<Statement> Parser::parseWhile() {
    expect(TokenKind::While);
    expect(TokenKind::LParen);

    auto condition = parseExpression();

    expect(TokenKind::RParen);
    expect(TokenKind::LBrace);

    auto body = parseBlock();

    return std::make_unique<WhileStmt>(
        std::move(condition),
        std::move(body)
    );
}

std::unique_ptr<Statement> Parser::parseIf() {

    expect(TokenKind::If);

    expect(TokenKind::LParen);

    auto condition =
        parseExpression();

    expect(TokenKind::RParen);

    expect(TokenKind::LBrace);

    auto thenBody =
        parseBlock();

    std::vector<std::unique_ptr<Statement>> elseBody;

    if (match(TokenKind::Else)) {

        expect(TokenKind::LBrace);

        elseBody =
            parseBlock();
    }

    return std::make_unique<IfStmt>(
        std::move(condition),
        std::move(thenBody),
        std::move(elseBody)
    );
}

std::vector<std::unique_ptr<Statement>>
Parser::parseBlock() {

    std::vector<std::unique_ptr<Statement>> body;

    while (peek().kind != TokenKind::RBrace &&
           peek().kind != TokenKind::End) {

        body.push_back(
            parseStatement()
        );
    }

    expect(TokenKind::RBrace);

    return body;
}

std::unique_ptr<Expr> Parser::parseExpression() {
    return parseEquality();
}

std::unique_ptr<Expr> Parser::parseEquality() {

    auto left =
        parseComparison();

    while (true) {

        if (match(TokenKind::EqualEqual)) {

            auto right =
                parseComparison();

            left = std::make_unique<BinaryExpr>(
                '=',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::NotEqual)) {

            auto right =
                parseComparison();

            left = std::make_unique<BinaryExpr>(
                '!',
                std::move(left),
                std::move(right)
            );
        }
        else {
            break;
        }
    }

    return left;
}

std::unique_ptr<Expr> Parser::parseComparison() {

    auto left =
        parseAdditive();

    while (true) {

        if (match(TokenKind::Greater)) {

            auto right =
                parseAdditive();

            left = std::make_unique<BinaryExpr>(
                '>',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::Less)) {

            auto right =
                parseAdditive();

            left = std::make_unique<BinaryExpr>(
                '<',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::GreaterEqual)) {

            auto right =
                parseAdditive();

            left = std::make_unique<BinaryExpr>(
                'G',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::LessEqual)) {

            auto right =
                parseAdditive();

            left = std::make_unique<BinaryExpr>(
                'L',
                std::move(left),
                std::move(right)
            );
        }
        else {
            break;
        }
    }

    return left;
}

std::unique_ptr<Expr> Parser::parseAdditive() {

    auto left =
        parseMultiplicative();

    while (true) {

        if (match(TokenKind::Plus)) {

            auto right =
                parseMultiplicative();

            left = std::make_unique<BinaryExpr>(
                '+',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::Minus)) {

            auto right =
                parseMultiplicative();

            left = std::make_unique<BinaryExpr>(
                '-',
                std::move(left),
                std::move(right)
            );
        }
        else {
            break;
        }
    }

    return left;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {

    auto left =
        parsePrimary();

    while (true) {

        if (match(TokenKind::Star)) {

            auto right =
                parsePrimary();

            left = std::make_unique<BinaryExpr>(
                '*',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::Slash)) {

            auto right =
                parsePrimary();

            left = std::make_unique<BinaryExpr>(
                '/',
                std::move(left),
                std::move(right)
            );
        }
        else {
            break;
        }
    }

    return left;
}

std::unique_ptr<Expr> Parser::parsePrimary() {

    if (peek().kind == TokenKind::Integer) {

        int value =
            std::stoi(advance().text);

        return std::make_unique<IntegerExpr>(
            value
        );
    }

    if (peek().kind == TokenKind::Identifier) {

        std::string name =
            advance().text;

        if (peek().kind == TokenKind::LParen) {
            auto args = parseArgList();

            return std::make_unique<CallExpr>(
                name,
                std::move(args)
            );
        }

        return std::make_unique<VariableExpr>(
            name
        );
    }

    if (match(TokenKind::LParen)) {

        auto expression =
            parseExpression();

        expect(TokenKind::RParen);

        return expression;
    }

    throw std::runtime_error(
        "Expected expression: " + peek().text
    );
}
