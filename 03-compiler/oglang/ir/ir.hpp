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
    ReturnI32
};

struct IRInstruction {
    OpCode opcode;

    ValueId destination;
    ValueId left;
    ValueId right;

    int value;
};

struct IRFunction {
    std::string name;
    std::vector<IRInstruction> instructions;
};
