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
        if (peek().kind == TokenKind::Struct) {
            program.structs.push_back(parseStructDecl());
        } else {
            program.push_back(parseFunction());
        }
    }

    return program;
}

StructDecl Parser::parseStructDecl() {
    expect(TokenKind::Struct);

    std::string name = expect(TokenKind::Identifier).text;

    expect(TokenKind::LBrace);

    std::vector<Param> fields;

    if (peek().kind != TokenKind::RBrace) {
        while (true) {
            std::string fieldName =
                expect(TokenKind::Identifier).text;

            expect(TokenKind::Colon);

            std::string fieldType = parseType();

            fields.push_back({fieldName, fieldType});

            if (match(TokenKind::Comma))
                continue;

            break;
        }
    }

    expect(TokenKind::RBrace);

    return StructDecl{name, std::move(fields)};
}

std::string Parser::parseType() {
    if (match(TokenKind::TypeI32)) return "i32";
    if (match(TokenKind::TypePtr)) return "ptr";
    if (match(TokenKind::TypeConstPtr)) return "constptr";

    // A bare identifier is accepted here as a possible struct type
    // name without checking it actually names a declared struct — the
    // parser stays permissive about type syntax; the type checker is
    // what rejects an identifier that isn't a real struct type.
    if (peek().kind == TokenKind::Identifier) {
        return advance().text;
    }

    throw std::runtime_error("Expected type");
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;

    expect(TokenKind::LParen);

    if (peek().kind != TokenKind::RParen) {
        while (true) {
            std::string name =
                expect(TokenKind::Identifier).text;

            expect(TokenKind::Colon);

            std::string type = parseType();

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

    std::string returnType = parseType();

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

        std::string type = parseType();

        if (match(TokenKind::LBracket)) {
            int size = std::stoi(expect(TokenKind::Integer).text);

            expect(TokenKind::RBracket);
            expect(TokenKind::Semicolon);

            return std::make_unique<ArrayDeclStmt>(name, type, size);
        }

        // A struct-typed local declares no initializer — every field
        // starts zero, the same as ArrayDeclStmt. This is the only
        // place a `let` is allowed to skip the `= expr` form: if the
        // type isn't followed by `[` (array) or `=` (plain value), it
        // must be `;` (struct). The type checker, not the parser,
        // confirms `type` actually names a declared struct.
        if (match(TokenKind::Semicolon)) {
            return std::make_unique<StructVarDeclStmt>(name, type);
        }

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

    if (peek().kind == TokenKind::Identifier &&
        peekNext().kind == TokenKind::LBracket)
        return parseIndexStore();

    if (peek().kind == TokenKind::Identifier &&
        peekNext().kind == TokenKind::Dot)
        return parseFieldStore();

    if (peek().kind == TokenKind::Star)
        return parseStore();

    throw std::runtime_error(
        "Unknown statement: " + peek().text
    );
}

std::unique_ptr<Statement> Parser::parseIndexStore() {
    std::string name = expect(TokenKind::Identifier).text;

    expect(TokenKind::LBracket);

    auto index = parseExpression();

    expect(TokenKind::RBracket);
    expect(TokenKind::Equal);

    auto value = parseExpression();

    expect(TokenKind::Semicolon);

    return std::make_unique<IndexStoreStmt>(
        name,
        std::move(index),
        std::move(value)
    );
}

std::unique_ptr<Statement> Parser::parseFieldStore() {
    std::string structVarName = expect(TokenKind::Identifier).text;

    expect(TokenKind::Dot);

    std::string fieldName = expect(TokenKind::Identifier).text;

    expect(TokenKind::Equal);

    auto value = parseExpression();

    expect(TokenKind::Semicolon);

    return std::make_unique<FieldStoreStmt>(
        structVarName,
        fieldName,
        std::move(value)
    );
}

std::unique_ptr<Statement> Parser::parseStore() {
    expect(TokenKind::Star);

    auto pointer = parseUnary();

    expect(TokenKind::Equal);

    auto value = parseExpression();

    expect(TokenKind::Semicolon);

    return std::make_unique<StoreStmt>(
        std::move(pointer),
        std::move(value)
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
        parseUnary();

    while (true) {

        if (match(TokenKind::Star)) {

            auto right =
                parseUnary();

            left = std::make_unique<BinaryExpr>(
                '*',
                std::move(left),
                std::move(right)
            );
        }
        else if (match(TokenKind::Slash)) {

            auto right =
                parseUnary();

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

std::unique_ptr<Expr> Parser::parseUnary() {

    if (match(TokenKind::Minus)) {
        auto operand = parseUnary();

        return std::make_unique<UnaryExpr>(
            '-',
            std::move(operand)
        );
    }

    if (match(TokenKind::Ampersand)) {
        std::string name = expect(TokenKind::Identifier).text;

        return std::make_unique<AddressOfExpr>(name);
    }

    if (match(TokenKind::Star)) {
        auto operand = parseUnary();

        return std::make_unique<DerefExpr>(
            std::move(operand)
        );
    }

    return parsePrimary();
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

        if (match(TokenKind::LBracket)) {
            auto index = parseExpression();

            expect(TokenKind::RBracket);

            return std::make_unique<IndexExpr>(
                name,
                std::move(index)
            );
        }

        if (match(TokenKind::Dot)) {
            std::string fieldName = expect(TokenKind::Identifier).text;

            return std::make_unique<FieldAccessExpr>(
                name,
                fieldName
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
