#include "type_checker.hpp"
#include <stdexcept>

Type TypeChecker::typeFromName(const std::string& name) {
    if (name == "i32")
        return {TypeKind::I32};

    throw std::runtime_error("Unknown type: " + name);
}

Type TypeChecker::checkExpression(const Expr* expr) {
    if (auto* integer = dynamic_cast<const IntegerExpr*>(expr)) {
        (void)integer;
        return {TypeKind::I32};
    }

    if (auto* variable = dynamic_cast<const VariableExpr*>(expr)) {
        auto it = variables.find(variable->name);

        if (it == variables.end())
            throw std::runtime_error(
                "Undefined variable: " + variable->name
            );

        return it->second;
    }

    if (auto* binary = dynamic_cast<const BinaryExpr*>(expr)) {
        Type left = checkExpression(binary->left.get());
        Type right = checkExpression(binary->right.get());

        if (binary->op == '+') {
            if (left.kind != TypeKind::I32 ||
                right.kind != TypeKind::I32) {
                throw std::runtime_error(
                    "Operator + requires i32 operands"
                );
            }

            return {TypeKind::I32};
        }
    }

    throw std::runtime_error("Unknown expression");
}

void TypeChecker::check(const Function& function) {
    variables.clear();

    Type returnType = typeFromName(function.returnType);

    for (const auto& statement : function.body) {

        if (auto* let =
                dynamic_cast<const LetStmt*>(statement.get())) {

            Type declared = typeFromName(let->type);
            Type actual = checkExpression(let->initializer.get());

            if (declared.kind != actual.kind)
                throw std::runtime_error(
                    "Initializer type mismatch for " + let->name
                );

            if (variables.contains(let->name))
                throw std::runtime_error(
                    "Variable already defined: " + let->name
                );

            variables[let->name] = declared;
            continue;
        }

        if (auto* ret =
                dynamic_cast<const ReturnStmt*>(statement.get())) {

            Type actual = checkExpression(ret->value.get());

            if (actual.kind != returnType.kind)
                throw std::runtime_error("Return type mismatch");

            continue;
        }

        throw std::runtime_error("Unknown statement");
    }
}
