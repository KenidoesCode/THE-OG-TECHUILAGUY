#include "lower.hpp"

#include <stdexcept>
#include <unordered_map>

IRFunction IRLowerer::lower(const Function& function) {
    IRFunction ir;

    std::unordered_map<std::string, ValueId> variables;

    for (const auto& statement : function.body) {

        if (auto* let =
                dynamic_cast<LetStmt*>(statement.get())) {

            auto* integer =
                dynamic_cast<IntegerExpr*>(
                    let->initializer.get()
                );

            if (!integer)
                throw std::runtime_error(
                    "Only integer initializers supported"
                );

            ValueId value = ir.createValue();

            ir.instructions.push_back({
                OpCode::ConstI32,
                value,
                -1,
                -1,
                integer->value
            });

            variables[let->name] = value;
            continue;
        }

        if (auto* ret =
                dynamic_cast<ReturnStmt*>(statement.get())) {

            auto* binary =
                dynamic_cast<BinaryExpr*>(
                    ret->value.get()
                );

            if (!binary || binary->op != '+')
                throw std::runtime_error(
                    "Only addition supported"
                );

            auto* left =
                dynamic_cast<VariableExpr*>(
                    binary->left.get()
                );

            auto* right =
                dynamic_cast<VariableExpr*>(
                    binary->right.get()
                );

            if (!left || !right)
                throw std::runtime_error(
                    "Expected variables"
                );

            if (!variables.contains(left->name) ||
                !variables.contains(right->name)) {
                throw std::runtime_error(
                    "Undefined variable in IR"
                );
            }

            ValueId result = ir.createValue();

            ir.instructions.push_back({
                OpCode::AddI32,
                result,
                variables[left->name],
                variables[right->name],
                0
            });

            ir.instructions.push_back({
                OpCode::ReturnI32,
                -1,
                result,
                -1,
                0
            });
        }
    }

    return ir;
}
