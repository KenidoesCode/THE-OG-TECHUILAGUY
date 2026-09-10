#pragma once

#include <cstdint>
#include <string>
#include <vector>

using ValueId = int;

enum class OpCode {
    ConstI32,
    AddI32,
    ReturnI32
};

struct IRInstruction {
    OpCode opcode;

    ValueId destination = -1;
    ValueId left = -1;
    ValueId right = -1;

    int32_t value = 0;
};

struct IRFunction {
    std::vector<IRInstruction> instructions;
    int nextValue = 0;

    ValueId createValue() {
        return nextValue++;
    }
};
