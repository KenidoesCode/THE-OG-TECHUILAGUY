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

    // name -> its element ValueIds, in index order. See
    // IRFunction::arrayGroups.
    std::unordered_map<std::string, std::vector<ValueId>> arrays;

    std::string freshLabel(const std::string& prefix);

    // Lowers `arr[index]` into the address arithmetic (element 0's
    // address, minus index * 8 — see arrayGroups' comment for why
    // slots are addressed in *decreasing* order as index increases)
    // shared by both the read (IndexExpr) and write (IndexStoreStmt)
    // paths, returning the ValueId of the computed effective address.
    ValueId lowerElementAddress(
        const std::string& arrayName,
        const Expr& indexExpr,
        IRFunction& ir
    );

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
