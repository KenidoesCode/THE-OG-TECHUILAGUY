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
};
