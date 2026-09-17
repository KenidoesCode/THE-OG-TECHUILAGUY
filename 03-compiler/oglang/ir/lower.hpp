#pragma once

#include "ir.hpp"
#include "../ast/ast.hpp"

#include <string>
#include <unordered_map>

class IRLowerer {
public:
    // structLayouts maps a struct type name to its field names in
    // declaration order — the field-index-to-byte-offset mapping is
    // program-wide (the same for every function), unlike `arrays`
    // below, which is per-function-local like any other variable.
    IRFunction lower(
        const Function& function,
        const std::unordered_map<std::string, std::vector<std::string>>&
            structLayouts = {}
    );

private:
    ValueId nextValue = 0;
    int nextLabel = 0;

    std::unordered_map<std::string, ValueId> variables;

    // name -> its element ValueIds, in index order. See
    // IRFunction::arrayGroups.
    std::unordered_map<std::string, std::vector<ValueId>> arrays;

    // struct type name -> field names, in declaration order. Set once
    // at the start of each lower() call from the constructor argument.
    std::unordered_map<std::string, std::vector<std::string>>
        structFieldOrder;

    // struct-typed local variable name -> its field ValueIds, ordered
    // exactly as structFieldOrder[itsType]. A separate namespace from
    // `variables`, mirroring `arrays`.
    std::unordered_map<std::string, std::vector<ValueId>> structVars;

    // struct-typed local variable name -> its struct type name (needed
    // to look up structFieldOrder for that variable's field order).
    std::unordered_map<std::string, std::string> structVarTypes;

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

    // Lowers `structVar.field` into the same shape of address
    // arithmetic as lowerElementAddress, except the "index" (the
    // field's position within the struct) is a compile-time constant
    // resolved from structFieldOrder rather than a runtime expression
    // — so there is no MulI32 (the offset is just fieldIndex * 8,
    // computed directly) and no BoundsCheckI32 (an unknown field name
    // is rejected earlier, by the type checker, not at run time).
    ValueId lowerFieldAddress(
        const std::string& structVarName,
        const std::string& fieldName,
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
