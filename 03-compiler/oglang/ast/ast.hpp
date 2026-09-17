#pragma once

#include <memory>
#include <string>
#include <vector>

struct Expr {
    virtual ~Expr() = default;
};

struct IntegerExpr : Expr {
    int value;

    explicit IntegerExpr(int value)
        : value(value) {}
};

struct VariableExpr : Expr {
    std::string name;

    explicit VariableExpr(std::string name)
        : name(std::move(name)) {}
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
        : op(op),
          left(std::move(left)),
          right(std::move(right)) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(
        std::string callee,
        std::vector<std::unique_ptr<Expr>> args
    )
        : callee(std::move(callee)),
          args(std::move(args)) {}
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

struct AssignStmt : Statement {
    std::string name;
    std::unique_ptr<Expr> value;

    AssignStmt(
        std::string name,
        std::unique_ptr<Expr> value
    )
        : name(std::move(name)),
          value(std::move(value)) {}
};

struct WhileStmt : Statement {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Statement>> body;

    WhileStmt(
        std::unique_ptr<Expr> condition,
        std::vector<std::unique_ptr<Statement>> body
    )
        : condition(std::move(condition)),
          body(std::move(body)) {}
};

struct IfStmt : Statement {
    std::unique_ptr<Expr> condition;

    std::vector<std::unique_ptr<Statement>> thenBody;
    std::vector<std::unique_ptr<Statement>> elseBody;

    IfStmt(
        std::unique_ptr<Expr> condition,
        std::vector<std::unique_ptr<Statement>> thenBody,
        std::vector<std::unique_ptr<Statement>> elseBody
    )
        : condition(std::move(condition)),
          thenBody(std::move(thenBody)),
          elseBody(std::move(elseBody)) {}
};

struct Param {
    std::string name;
    std::string type;
};

struct Function {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    std::vector<std::unique_ptr<Statement>> body;
};

using Program = std::vector<Function>;
