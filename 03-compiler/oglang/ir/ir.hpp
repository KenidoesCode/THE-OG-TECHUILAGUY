#pragma once

#include <string>
#include <vector>

using ValueId = int;

enum class OpCode {
    ConstI32,
    AddI32,
    SubI32,
    MulI32,
    DivI32,

    CmpEqI32,
    CmpNeI32,
    CmpGtI32,
    CmpLtI32,
    CmpGeI32,
    CmpLeI32,

    // destination = value of the `value`-th incoming parameter
    // (0-indexed, ABI-assigned by the codegen's calling convention).
    ParamI32,

    // destination = call(label, args...)
    Call,

    // No destination. `label` names the target of Jump/JumpIfZero,
    // or marks this position for one of them.
    Label,
    Jump,
    JumpIfZero,

    ReturnI32
};

struct IRInstruction {
    OpCode opcode;

    ValueId destination;
    ValueId left;
    ValueId right;

    int value;

    // Call arguments (Call only) and jump/label targets or call callee
    // (Call, Label, Jump, JumpIfZero).
    std::vector<ValueId> args;
    std::string label;
};

struct IRFunction {
    std::string name;
    int paramCount = 0;
    std::vector<IRInstruction> instructions;
};
