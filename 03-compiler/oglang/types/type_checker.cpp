#include "type_checker.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class Checker {
public:
    std::unordered_map<std::string, std::string> variables;

    std::string checkExpr(const Expr& expr) {
        if (dynamic_cast<const IntegerExpr*>(&expr))
            return "i32";

        if (auto* variable =
                dynamic_cast<const VariableExpr*>(&expr)) {

            auto it = variables.find(variable->name);

            if (it == variables.end()) {
                throw std::runtime_error(
                    "Unknown variable: " + variable->name
                );
            }

            return it->second;
        }

        if (auto* binary =
                dynamic_cast<const BinaryExpr*>(&expr)) {

            std::string left = checkExpr(*binary->left);
            std::string right = checkExpr(*binary->right);

            if (left != "i32" || right != "i32") {
                throw std::runtime_error(
                    "Binary operator requires i32 operands"
                );
            }

            switch (binary->op) {
                case '+':
                case '-':
                case '*':
                case '/':
                    return "i32";

                default:
                    throw std::runtime_error(
                        "Unknown binary operator"
                    );
            }
        }

        throw std::runtime_error("Unknown expression");
    }
};

}

void TypeChecker::check(const Function& function) {
    Checker checker;

    for (const auto& statement : function.body) {

        if (auto* letStmt =
                dynamic_cast<const LetStmt*>(statement.get())) {

            std::string actual =
                checker.checkExpr(*letStmt->initializer);

            if (actual != letStmt->type) {
                throw std::runtime_error(
                    "Type mismatch for variable: " +
                    letStmt->name
                );
            }

            checker.variables[letStmt->name] =
                letStmt->type;

            continue;
        }

        if (auto* returnStmt =
                dynamic_cast<const ReturnStmt*>(statement.get())) {

            std::string actual =
                checker.checkExpr(*returnStmt->value);

            if (actual != function.returnType) {
                throw std::runtime_error(
                    "Return type mismatch"
                );
            }

            continue;
        }

        throw std::runtime_error("Unknown statement");
    }
}
