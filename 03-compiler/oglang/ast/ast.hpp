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

// structVar.field — reads one named field of a local struct-typed
// variable. Restricted to a plain (structVarName, fieldName) pair, not
// a composable base expression, mirroring IndexExpr's own restriction
// to a bare array name — nothing in this compiler supports chained
// member-like access (a.b.c) yet.
struct FieldAccessExpr : Expr {
    std::string structVarName;
    std::string fieldName;

    FieldAccessExpr(std::string structVarName, std::string fieldName)
        : structVarName(std::move(structVarName)),
          fieldName(std::move(fieldName)) {}
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

// let name: StructType; — declares a struct-typed local variable with
// every field zero-initialized. No initializer expression, exactly
// like ArrayDeclStmt: structs have no literal-initializer syntax yet.
struct StructVarDeclStmt : Statement {
    std::string name;
    std::string structType;

    StructVarDeclStmt(std::string name, std::string structType)
        : name(std::move(name)),
          structType(std::move(structType)) {}
};

// structVar.field = value; — writes one named field of a local
// struct-typed variable.
struct FieldStoreStmt : Statement {
    std::string structVarName;
    std::string fieldName;
    std::unique_ptr<Expr> value;

    FieldStoreStmt(
        std::string structVarName,
        std::string fieldName,
        std::unique_ptr<Expr> value
    )
        : structVarName(std::move(structVarName)),
          fieldName(std::move(fieldName)),
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

// struct Name { field1: type1, field2: type2, ... } — a top-level
// struct type definition. Fields are laid out contiguously, in
// declaration order, exactly like a fixed-size array's elements (see
// IRLowerer/RegisterAllocator's arrayGroups); the only difference is a
// struct is addressed by named, compile-time-constant field offsets
// instead of a runtime index, so field access needs no bounds check.
// A field's own type reuses Param's shape (name + type string).
struct StructDecl {
    std::string name;
    std::vector<Param> fields;
};

// A program is one or more struct type definitions plus one or more
// functions. Struct definitions are program-wide (visible to every
// function regardless of declaration order, same as function
// signatures); Program keeps a vector<Function>-compatible interface
// (size/operator[]/begin/end/push_back) so the many existing call
// sites written when Program was a bare std::vector<Function> keep
// working unchanged against the .functions half.
struct Program {
    std::vector<StructDecl> structs;
    std::vector<Function> functions;

    size_t size() const { return functions.size(); }
    bool empty() const { return functions.empty(); }
    void push_back(Function function) {
        functions.push_back(std::move(function));
    }

    Function& operator[](size_t index) { return functions[index]; }
    const Function& operator[](size_t index) const {
        return functions[index];
    }

    std::vector<Function>::iterator begin() { return functions.begin(); }
    std::vector<Function>::iterator end() { return functions.end(); }
    std::vector<Function>::const_iterator begin() const {
        return functions.begin();
    }
    std::vector<Function>::const_iterator end() const {
        return functions.end();
    }
};
