#pragma once

#include "ir.hpp"
#include "../ast/ast.hpp"

#include <unordered_map>

class IRLowerer {
public:
    IRFunction lower(const Function& function);

private:
    ValueId nextValue = 0;

    std::unordered_map<std::string, ValueId> variables;

    void lowerStatement(
        const Statement& statement,
        IRFunction& ir
    );

    ValueId lowerExpr(
        const Expr& expr,
        IRFunction& ir
    );
};
