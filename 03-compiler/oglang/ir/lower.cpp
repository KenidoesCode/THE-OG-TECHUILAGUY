#include "lower.hpp"

#include <stdexcept>
#include <string>

IRFunction IRLowerer::lower(const Function& function) {
    IRFunction ir;

    int temporary = 0;

    auto newTemp = [&]() {
        return "%t" + std::to_string(temporary++);
    };

    for (const auto& statement : function.body) {

        if (auto* let =
                dynamic_cast<LetStmt*>(statement.get())) {

            auto* integer =
                dynamic_cast<IntegerExpr*>(
                    let->initializer.get()
                );

            if (!integer)
                throw std::runtime_error(
                    "Unsupported initializer"
                );

            ir.instructions.push_back({
                OpCode::ConstI32,
                let->name,
                "",
                "",
                integer->value
            });

            continue;
        }

        if (auto* ret =
                dynamic_cast<ReturnStmt*>(statement.get())) {

            auto* binary =
                dynamic_cast<BinaryExpr*>(ret->value.get());

            if (!binary || binary->op != '+')
                throw std::runtime_error(
                    "Unsupported return expression"
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

            std::string result = newTemp();

            ir.instructions.push_back({
                OpCode::AddI32,
                result,
                left->name,
                right->name,
                0
            });

            ir.instructions.push_back({
                OpCode::ReturnI32,
                "",
                result,
                "",
                0
            });
        }
    }

    return ir;
}
