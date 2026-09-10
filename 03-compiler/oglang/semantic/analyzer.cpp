#include "analyzer.hpp"
#include <stdexcept>
#include <unordered_map>

void SemanticAnalyzer::analyze(const Function& function) {
    if (function.returnType != "i32")
        throw std::runtime_error("Unsupported return type");

    std::unordered_map<std::string, std::string> variables;

    for (const auto& statement : function.body) {

        if (auto* let = dynamic_cast<LetStmt*>(statement.get())) {
            if (variables.contains(let->name))
                throw std::runtime_error(
                    "Variable already defined: " + let->name
                );

            if (let->type != "i32")
                throw std::runtime_error(
                    "Unsupported variable type"
                );

            variables[let->name] = let->type;
            continue;
        }

        if (auto* ret = dynamic_cast<ReturnStmt*>(statement.get())) {

            if (auto* integer =
                    dynamic_cast<IntegerExpr*>(ret->value.get())) {
                (void)integer;
                continue;
            }

            if (auto* variable =
                    dynamic_cast<VariableExpr*>(ret->value.get())) {

                if (!variables.contains(variable->name))
                    throw std::runtime_error(
                        "Undefined variable: " + variable->name
                    );

                continue;
            }

            if (auto* binary =
                    dynamic_cast<BinaryExpr*>(ret->value.get())) {

                auto checkVariable =
                    [&](const Expr* expr) {
                        if (auto* v =
                                dynamic_cast<const VariableExpr*>(expr)) {

                            if (!variables.contains(v->name))
                                throw std::runtime_error(
                                    "Undefined variable: " + v->name
                                );
                        }
                    };

                checkVariable(binary->left.get());
                checkVariable(binary->right.get());
                continue;
            }

            throw std::runtime_error("Invalid return expression");
        }

        throw std::runtime_error("Unknown statement");
    }
}
