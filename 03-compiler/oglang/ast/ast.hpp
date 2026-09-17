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

struct UnaryExpr : Expr {
    char op;
    std::unique_ptr<Expr> operand;

    UnaryExpr(char op, std::unique_ptr<Expr> operand)
        : op(op), operand(std::move(operand)) {}
};

// &name — takes the address of a named local variable or parameter.
// Restricted to a plain name (not an arbitrary expression) so it's
// always statically clear which variable must be forced to live in a
// stable memory location rather than a register — see IRLowerer's
// addressTakenValues.
struct AddressOfExpr : Expr {
    std::string name;

    explicit AddressOfExpr(std::string name)
        : name(std::move(name)) {}
};

// *expr — reads the i32 value a `ptr` expression points to.
struct DerefExpr : Expr {
    std::unique_ptr<Expr> pointer;

    explicit DerefExpr(std::unique_ptr<Expr> pointer)
        : pointer(std::move(pointer)) {}
};

// arr[index] — reads one element of a fixed-size local array.
struct IndexExpr : Expr {
    std::string arrayName;
    std::unique_ptr<Expr> index;

    IndexExpr(std::string arrayName, std::unique_ptr<Expr> index)
        : arrayName(std::move(arrayName)), index(std::move(index)) {}
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

// *pointer = value; — writes through a pointer.
struct StoreStmt : Statement {
    std::unique_ptr<Expr> pointer;
    std::unique_ptr<Expr> value;

    StoreStmt(
        std::unique_ptr<Expr> pointer,
        std::unique_ptr<Expr> value
    )
        : pointer(std::move(pointer)),
          value(std::move(value)) {}
};

// let name: elementType[size]; — declares a fixed-size, zero-initialized
// local array. No initializer expression: arrays start at all zeros.
struct ArrayDeclStmt : Statement {
    std::string name;
    std::string elementType;
    int size;

    ArrayDeclStmt(std::string name, std::string elementType, int size)
        : name(std::move(name)),
          elementType(std::move(elementType)),
          size(size) {}
};

// arr[index] = value; — writes one element of a fixed-size local array.
struct IndexStoreStmt : Statement {
    std::string arrayName;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;

    IndexStoreStmt(
        std::string arrayName,
        std::unique_ptr<Expr> index,
        std::unique_ptr<Expr> value
    )
        : arrayName(std::move(arrayName)),
          index(std::move(index)),
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
