#pragma once

#include "../ast/ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct FunctionSignature {
    std::vector<std::string> paramTypes;
    std::string returnType;
};

class TypeChecker {
public:
    // Type-checks an entire program. Builds a signature table first so
    // functions may call each other regardless of declaration order
    // (including recursively and self-recursively). Struct
    // declarations are collected the same way, before any function
    // body is checked, so a struct may be used regardless of where in
    // the file it's declared relative to the functions that use it.
    void check(const Program& program);

private:
    std::unordered_map<std::string, FunctionSignature> signatures;

    // struct name -> (field name -> field type), in declaration order
    // is not needed here (only IR lowering needs field order for byte
    // offsets); the type checker only needs to answer "does this
    // struct have a field of this name, and what's its type."
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> structs;
};
