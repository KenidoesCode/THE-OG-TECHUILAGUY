#pragma once

#include "../ast/ast.hpp"
#include "type.hpp"

#include <string>
#include <unordered_map>

class TypeChecker {
public:
    void check(const Function& function);

private:
    std::unordered_map<std::string, Type> variables;

    Type checkExpression(const Expr* expr);
    Type typeFromName(const std::string& name);
};
