#pragma once

#include "../analysis/interference.hpp"
#include <string>
#include <unordered_map>
#include <vector>

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
    //
    // `forcedSpills` are values that must never land in a register
    // regardless of what coloring would otherwise choose — namely,
    // variables whose address was taken (AddressOfExpr), since only a
    // stack slot has a stable address a pointer could actually hold.
    // They're given a slot up front and removed from the graph before
    // coloring runs, so they impose no register-color constraint on
    // anything else either.
    //
    // `arrayGroups` are stronger still: each inner vector is one
    // array's element ValueIds, in index order, and must receive
    // *contiguous* slot numbers in that exact order — a plain
    // forcedSpill only guarantees each value gets some slot, not that
    // a whole group's slots are adjacent, which array index arithmetic
    // (element address = element-0's address plus a fixed multiple of
    // the index) depends on.
    RegisterAllocation allocate(
        const InterferenceGraph& graph,
        const std::vector<ValueId>& forcedSpills = {},
        const std::vector<std::vector<ValueId>>& arrayGroups = {}
    );
};
