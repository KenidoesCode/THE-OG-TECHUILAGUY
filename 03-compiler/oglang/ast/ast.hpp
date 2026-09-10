#pragma once

#include <memory>
#include <string>
#include <vector>

struct Expr {
    virtual ~Expr() = default;
};

struct IntegerExpr : Expr {
    int value;
    explicit IntegerExpr(int value) : value(value) {}
};

struct VariableExpr : Expr {
    std::string name;
    explicit VariableExpr(std::string name) : name(std::move(name)) {}
};

struct BinaryExpr : Expr {
    char op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(
        char op,
        std::unique_ptr<Expr> left,
        std::unique_ptr<Expr> right
    )
        : op(op), left(std::move(left)), right(std::move(right)) {}
};

struct Statement {
    virtual ~Statement() = default;
};

struct LetStmt : Statement {
    std::string name;
    std::string type;
    std::unique_ptr<Expr> initializer;

    LetStmt(
        std::string name,
        std::string type,
        std::unique_ptr<Expr> initializer
    )
        : name(std::move(name)),
          type(std::move(type)),
          initializer(std::move(initializer)) {}
};

struct ReturnStmt : Statement {
    std::unique_ptr<Expr> value;

    explicit ReturnStmt(std::unique_ptr<Expr> value)
        : value(std::move(value)) {}
};

struct Function {
    std::string name;
    std::string returnType;
    std::vector<std::unique_ptr<Statement>> body;
};
