#include "lower.hpp"

#include <stdexcept>

std::string IRLowerer::freshLabel(const std::string& prefix) {
    return prefix + std::to_string(nextLabel++);
}

IRFunction IRLowerer::lower(const Function& function) {
    IRFunction ir;
    ir.name = function.name;
    ir.paramCount = static_cast<int>(function.params.size());

    nextValue = 0;
    nextLabel = 0;
    variables.clear();

    for (size_t i = 0; i < function.params.size(); ++i) {
        ValueId dst = nextValue++;

        ir.instructions.push_back({
            OpCode::ParamI32,
            dst,
            -1,
            -1,
            static_cast<int>(i),
            {},
            ""
        });

        variables[function.params[i].name] = dst;
    }

    lowerBlock(function.body, ir);

    return ir;
}

void IRLowerer::lowerBlock(
    const std::vector<std::unique_ptr<Statement>>& body,
    IRFunction& ir
) {
    for (const auto& statement : body) {
        lowerStatement(*statement, ir);
    }
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

    if (auto* assignStmt = dynamic_cast<const AssignStmt*>(&statement)) {
        auto it = variables.find(assignStmt->name);

        if (it == variables.end()) {
            throw std::runtime_error(
                "Assignment to unknown variable: " + assignStmt->name
            );
        }

        ValueId newValue = lowerExpr(*assignStmt->value, ir);

        // Written into the variable's existing ValueId in place,
        // deliberately not rebound to newValue — see MoveI32's comment
        // in ir.hpp for why.
        ir.instructions.push_back({
            OpCode::MoveI32,
            it->second,
            newValue,
            -1,
            0,
            {},
            ""
        });

        return;
    }

    if (auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        std::string loopStart = freshLabel(".Lloop");
        std::string loopEnd = freshLabel(".Lloopend");

        IRInstruction startLabel{
            OpCode::Label, -1, -1, -1, 0, {}, loopStart
        };
        ir.instructions.push_back(startLabel);

        ValueId cond = lowerExpr(*whileStmt->condition, ir);

        IRInstruction branch{
            OpCode::JumpIfZero, -1, cond, -1, 0, {}, loopEnd
        };
        ir.instructions.push_back(branch);

        lowerBlock(whileStmt->body, ir);

        IRInstruction backEdge{
            OpCode::Jump, -1, -1, -1, 0, {}, loopStart
        };
        ir.instructions.push_back(backEdge);

        IRInstruction endLabel{
            OpCode::Label, -1, -1, -1, 0, {}, loopEnd
        };
        ir.instructions.push_back(endLabel);

        return;
    }

    if (auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        ValueId value = lowerExpr(*returnStmt->value, ir);

        ir.instructions.push_back({
            OpCode::ReturnI32,
            -1,
            value,
            -1,
            0,
            {},
            ""
        });

        return;
    }

    if (auto* ifStmt = dynamic_cast<const IfStmt*>(&statement)) {
        ValueId cond = lowerExpr(*ifStmt->condition, ir);

        std::string elseLabel = freshLabel(".Lelse");
        std::string endLabel = freshLabel(".Lend");

        IRInstruction branch{
            OpCode::JumpIfZero, -1, cond, -1, 0, {}, ""
        };
        branch.label = ifStmt->elseBody.empty() ? endLabel : elseLabel;
        ir.instructions.push_back(branch);

        lowerBlock(ifStmt->thenBody, ir);

        if (!ifStmt->elseBody.empty()) {
            IRInstruction jump{OpCode::Jump, -1, -1, -1, 0, {}, ""};
            jump.label = endLabel;
            ir.instructions.push_back(jump);

            IRInstruction elseLabelInst{OpCode::Label, -1, -1, -1, 0, {}, ""};
            elseLabelInst.label = elseLabel;
            ir.instructions.push_back(elseLabelInst);

            lowerBlock(ifStmt->elseBody, ir);
        }

        IRInstruction endLabelInst{OpCode::Label, -1, -1, -1, 0, {}, ""};
        endLabelInst.label = endLabel;
        ir.instructions.push_back(endLabelInst);

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
            integer->value,
            {},
            ""
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

    if (auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
        if (unary->op != '-') {
            throw std::runtime_error("Unsupported unary operator");
        }

        ValueId operand = lowerExpr(*unary->operand, ir);

        ValueId zero = nextValue++;
        ir.instructions.push_back({
            OpCode::ConstI32, zero, -1, -1, 0, {}, ""
        });

        ValueId dst = nextValue++;
        ir.instructions.push_back({
            OpCode::SubI32, dst, zero, operand, 0, {}, ""
        });

        return dst;
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        std::vector<ValueId> argValues;

        for (const auto& arg : call->args) {
            argValues.push_back(lowerExpr(*arg, ir));
        }

        ValueId dst = nextValue++;

        IRInstruction inst{OpCode::Call, dst, -1, -1, 0, {}, ""};
        inst.args = std::move(argValues);
        inst.label = call->callee;

        ir.instructions.push_back(inst);

        return dst;
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

            case '=':
                opcode = OpCode::CmpEqI32;
                break;

            case '!':
                opcode = OpCode::CmpNeI32;
                break;

            case '>':
                opcode = OpCode::CmpGtI32;
                break;

            case '<':
                opcode = OpCode::CmpLtI32;
                break;

            case 'G':
                opcode = OpCode::CmpGeI32;
                break;

            case 'L':
                opcode = OpCode::CmpLeI32;
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
            0,
            {},
            ""
        });

        return dst;
    }

    throw std::runtime_error("Unsupported expression");
}
