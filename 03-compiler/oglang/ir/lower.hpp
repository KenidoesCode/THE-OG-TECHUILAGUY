#pragma once

#include "ir.hpp"
#include "../ast/ast.hpp"

#include <string>
#include <unordered_map>

class IRLowerer {
public:
    IRFunction lower(const Function& function);

private:
    ValueId nextValue = 0;
    int nextLabel = 0;

    std::unordered_map<std::string, ValueId> variables;

    std::string freshLabel(const std::string& prefix);

    void lowerStatement(
        const Statement& statement,
        IRFunction& ir
    );

    void lowerBlock(
        const std::vector<std::unique_ptr<Statement>>& body,
        IRFunction& ir
    );

    ValueId lowerExpr(
        const Expr& expr,
        IRFunction& ir
    );
};
