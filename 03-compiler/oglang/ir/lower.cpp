#include "lower.hpp"

#include <stdexcept>

IRFunction IRLowerer::lower(const Function& function) {
    IRFunction ir;
    ir.name = function.name;

    nextValue = 0;
    variables.clear();

    for (const auto& statement : function.body) {
        lowerStatement(*statement, ir);
    }

    return ir;
}

void IRLowerer::lowerStatement(
    const Statement& statement,
    IRFunction& ir
) {
    if (auto* letStmt = dynamic_cast<const LetStmt*>(&statement)) {
        ValueId value = lowerExpr(*letStmt->initializer, ir);
        variables[letStmt->name] = value;
        return;
    }

    if (auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        ValueId value = lowerExpr(*returnStmt->value, ir);

        ir.instructions.push_back({
            OpCode::ReturnI32,
            -1,
            value,
            -1,
            0
        });

        return;
    }

    throw std::runtime_error("Unsupported statement");
}

ValueId IRLowerer::lowerExpr(
    const Expr& expr,
    IRFunction& ir
) {
    if (auto* integer =
            dynamic_cast<const IntegerExpr*>(&expr)) {

        ValueId dst = nextValue++;

        ir.instructions.push_back({
            OpCode::ConstI32,
            dst,
            -1,
            -1,
            integer->value
        });

        return dst;
    }

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

        ValueId left = lowerExpr(*binary->left, ir);
        ValueId right = lowerExpr(*binary->right, ir);

        ValueId dst = nextValue++;

        OpCode opcode;

        switch (binary->op) {
            case '+':
                opcode = OpCode::AddI32;
                break;

            case '-':
                opcode = OpCode::SubI32;
                break;

            case '*':
                opcode = OpCode::MulI32;
                break;

            case '/':
                opcode = OpCode::DivI32;
                break;

            default:
                throw std::runtime_error(
                    "Unsupported binary operator"
                );
        }

        ir.instructions.push_back({
            opcode,
            dst,
            left,
            right,
            0
        });

        return dst;
    }

    throw std::runtime_error("Unsupported expression");
}
