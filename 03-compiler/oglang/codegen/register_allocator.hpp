#pragma once

#include "../analysis/interference.hpp"
#include <string>
#include <unordered_map>

// Values that fit in a physical register get one; anything left over is
// assigned a stack slot instead of causing allocation to fail outright.
// `spillSlotCount` tells the codegen how many 8-byte slots to reserve in
// the function's stack frame.
struct RegisterAllocation {
    std::unordered_map<ValueId, std::string> registers;
    std::unordered_map<ValueId, int> spillSlots;
    int spillSlotCount = 0;
};

class RegisterAllocator {
public:
    // Standard Chaitin-style graph coloring: repeatedly simplify away
    // any node with fewer neighbors than there are registers; if none
    // exists, optimistically remove the highest-degree node as a
    // potential spill and keep going. Colors are then assigned in
    // reverse simplify order; a potential spill only becomes an actual
    // one (a stack slot instead of a register) if, once its neighbors
    // are colored, no register is actually free for it.
    RegisterAllocation allocate(const InterferenceGraph& graph);
};
