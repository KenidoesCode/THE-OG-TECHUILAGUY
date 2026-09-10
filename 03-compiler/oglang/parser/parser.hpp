#pragma once

#include "../lexer/token.hpp"
#include "../ast/ast.hpp"
#include <memory>
#include <vector>

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    Function parseFunction();

private:
    const std::vector<Token>& tokens;
    std::size_t current = 0;

    const Token& peek() const;
    const Token& advance();
    bool check(TokenType type) const;
    const Token& expect(TokenType type);

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Stmt> parseStatement();
};
