#include "type_checker.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

class Checker {
public:
    explicit Checker(
        const std::unordered_map<std::string, FunctionSignature>& signatures
    )
        : signatures(signatures) {}

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

        if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
            auto it = signatures.find(call->callee);

            if (it == signatures.end()) {
                throw std::runtime_error(
                    "Call to undefined function: " + call->callee
                );
            }

            const FunctionSignature& sig = it->second;

            if (call->args.size() != sig.paramTypes.size()) {
                throw std::runtime_error(
                    "Function '" + call->callee + "' expects " +
                    std::to_string(sig.paramTypes.size()) +
                    " argument(s), got " +
                    std::to_string(call->args.size())
                );
            }

            for (size_t i = 0; i < call->args.size(); ++i) {
                std::string argType = checkExpr(*call->args[i]);

                if (argType != sig.paramTypes[i]) {
                    throw std::runtime_error(
                        "Argument " + std::to_string(i + 1) +
                        " to '" + call->callee +
                        "' has wrong type"
                    );
                }
            }

            return sig.returnType;
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
                case '=':
                case '!':
                case '>':
                case '<':
                case 'G':
                case 'L':
                    // Comparisons also produce i32 (0 or 1); OGLang
                    // has no distinct boolean type yet.
                    return "i32";

                default:
                    throw std::runtime_error(
                        "Unknown binary operator"
                    );
            }
        }

        throw std::runtime_error("Unknown expression");
    }

    void checkBlock(
        const std::vector<std::unique_ptr<Statement>>& body,
        const std::string& functionReturnType
    ) {
        for (const auto& statement : body) {
            checkStatement(*statement, functionReturnType);
        }
    }

    void checkStatement(
        const Statement& statement,
        const std::string& functionReturnType
    ) {
        if (auto* letStmt =
                dynamic_cast<const LetStmt*>(&statement)) {

            std::string actual = checkExpr(*letStmt->initializer);

            if (actual != letStmt->type) {
                throw std::runtime_error(
                    "Type mismatch for variable: " + letStmt->name
                );
            }

            variables[letStmt->name] = letStmt->type;
            return;
        }

        if (auto* returnStmt =
                dynamic_cast<const ReturnStmt*>(&statement)) {

            std::string actual = checkExpr(*returnStmt->value);

            if (actual != functionReturnType) {
                throw std::runtime_error("Return type mismatch");
            }

            return;
        }

        if (auto* ifStmt =
                dynamic_cast<const IfStmt*>(&statement)) {

            std::string condType = checkExpr(*ifStmt->condition);

            if (condType != "i32") {
                throw std::runtime_error(
                    "If condition must be i32 (0 is false, nonzero is true)"
                );
            }

            checkBlock(ifStmt->thenBody, functionReturnType);
            checkBlock(ifStmt->elseBody, functionReturnType);
            return;
        }

        throw std::runtime_error("Unknown statement");
    }

private:
    const std::unordered_map<std::string, FunctionSignature>& signatures;
};

}  // namespace

void TypeChecker::check(const Program& program) {
    signatures.clear();

    for (const auto& function : program) {
        if (signatures.contains(function.name)) {
            throw std::runtime_error(
                "Function redefined: " + function.name
            );
        }

        FunctionSignature sig;
        for (const auto& param : function.params) {
            sig.paramTypes.push_back(param.type);
        }
        sig.returnType = function.returnType;

        signatures[function.name] = std::move(sig);
    }

    if (!signatures.contains("main")) {
        throw std::runtime_error(
            "Program has no 'main' function"
        );
    }

    for (const auto& function : program) {
        Checker checker(signatures);

        for (const auto& param : function.params) {
            checker.variables[param.name] = param.type;
        }

        checker.checkBlock(function.body, function.returnType);
    }
}
