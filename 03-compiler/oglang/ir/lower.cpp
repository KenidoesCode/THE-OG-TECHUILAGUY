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

        if (auto* let = dynamic_cast<LetStmt*>(statement.get())) {
            auto* integer =
                dynamic_cast<IntegerExpr*>(let->initializer.get());

            if (!integer)
                throw std::runtime_error(
                    "Only integer initializers supported"
                );

            ir.instructions.push_back({
                OpCode::ConstI32,
                let->name,
                "",
                "",
                static_cast<int32_t>(integer->value)
            });

            continue;
        }

        if (auto* ret =
                dynamic_cast<ReturnStmt*>(statement.get())) {

            if (auto* integer =
                    dynamic_cast<IntegerExpr*>(ret->value.get())) {

                ir.instructions.push_back({
                    OpCode::ConstI32,
                    "%return",
                    "",
                    "",
                    static_cast<int32_t>(integer->value)
                });

                ir.instructions.push_back({
                    OpCode::ReturnI32,
                    "",
                    "%return",
                    "",
                    0
                });

                continue;
            }

            if (auto* binary =
                    dynamic_cast<BinaryExpr*>(ret->value.get())) {

                auto* left =
                    dynamic_cast<VariableExpr*>(binary->left.get());

                auto* right =
                    dynamic_cast<VariableExpr*>(binary->right.get());

                if (!left || !right || binary->op != '+')
                    throw std::runtime_error(
                        "Unsupported binary expression"
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
    }

    return ir;
}
