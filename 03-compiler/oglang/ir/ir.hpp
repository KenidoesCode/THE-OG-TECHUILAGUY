#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class OpCode {
    ConstI32,
    AddI32,
    ReturnI32
};

struct IRInstruction {
    OpCode opcode;
    std::string destination;
    std::string left;
    std::string right;
    int32_t value = 0;
};

struct IRFunction {
    std::vector<IRInstruction> instructions;
};
