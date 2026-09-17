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
    // (including recursively and self-recursively).
    void check(const Program& program);

private:
    std::unordered_map<std::string, FunctionSignature> signatures;
};
