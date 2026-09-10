#pragma once

#include "../ast/ast.hpp"
#include "../lexer/token.hpp"

#include <vector>

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    Function parseFunction();

private:
    const std::vector<Token>& tokens;
    size_t current = 0;

    const Token& peek() const;
    const Token& advance();

    bool match(TokenKind kind);
    const Token& expect(TokenKind kind);

    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseIf();

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parsePrimary();

    std::vector<std::unique_ptr<Statement>> parseBlock();
};
