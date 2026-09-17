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

    // destination = left, in place. Used for mutable-variable
    // assignment (AssignStmt) so a variable keeps the same ValueId —
    // and therefore the same register/spill slot — across its whole
    // lifetime instead of being rebound to a fresh SSA-style value on
    // every write. That's what lets a loop's condition check (lowered
    // once, before the body) and the body's updates to the same
    // variable agree on where its current value actually lives.
    MoveI32,

    // destination = call(label, args...)
    Call,

    // destination = address of `left` (a variable's own ValueId, not a
    // register/temporary — see IRFunction::addressTakenValues). Only
    // meaningful because the register allocator is told to force
    // `left` into a stable stack slot rather than a register whenever
    // it appears here; codegen then computes a real address (leal)
    // into that slot.
    AddressOfI32,

    // destination = *left (left holds a pointer value).
    LoadI32,

    // *left = right. No destination: this instruction only writes
    // memory, it doesn't produce a new value.
    StoreI32,

    // destination = left - right, where `left` and `destination` are
    // pointer values (64-bit) and `right` is a plain i32 byte offset.
    // Used for array element addressing. Deliberately a distinct
    // opcode from SubI32, not a reuse of it: SubI32's codegen operates
    // on 32-bit registers throughout, which would silently truncate a
    // real 64-bit pointer value — exactly the same class of bug
    // AddressOfI32/LoadI32/StoreI32 already had to avoid.
    PtrSubI32,

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

    // ValueIds that had their address taken (via AddressOfExpr) at
    // some point in this function. The register allocator must never
    // put these in a physical register — only a stack slot has a
    // stable address a pointer could actually hold — regardless of
    // what graph coloring would otherwise choose for them.
    std::vector<ValueId> addressTakenValues;

    // Each entry is one array's element ValueIds, in index order
    // (element 0 first). The register allocator gives every ValueId in
    // one group a spill slot, in *reserved contiguous order*, so that
    // element i's address can be computed as a fixed offset from
    // element 0's address (see IRLowerer's index-expression lowering)
    // — a plain per-value forced spill (like addressTakenValues) only
    // guarantees each value has *a* slot, not that a whole group's
    // slots are contiguous and in a known order relative to each
    // other.
    std::vector<std::vector<ValueId>> arrayGroups;
};
