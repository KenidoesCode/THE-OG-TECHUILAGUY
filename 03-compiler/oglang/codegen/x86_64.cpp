#include "x86_64.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

// x86-64 does not encode a 32-bit-width push/pop of a GPR (push/pop
// default to 64-bit operand size in long mode). We only ever need to
// save/restore the 32-bit value our language operates on, so we push
// and pop the containing 64-bit register instead; the upper 32 bits
// are never read back by generated code.
std::string to64(const std::string& reg32In) {
    const std::string& reg32 =
        (!reg32In.empty() && reg32In[0] == '%')
            ? reg32In.substr(1)
            : reg32In;

    if (reg32 == "eax") return "rax";
    if (reg32 == "ecx") return "rcx";
    if (reg32 == "edx") return "rdx";
    if (reg32 == "esi") return "rsi";
    if (reg32 == "edi") return "rdi";
    if (reg32 == "ebx") return "rbx";
    if (reg32 == "r8d") return "r8";

    throw std::runtime_error("No 64-bit alias for register: " + reg32In);
}

// Restricted System V AMD64 integer argument registers, in order.
// Limited to 4 slots: edi is outside the general allocator's pool, but
// esi/edx/ecx overlap with it, which the stack-mediated marshaling
// below resolves without needing a real parallel-move algorithm.
const std::vector<std::string> kAbiArgRegisters = {
    "edi", "esi", "edx", "ecx"
};

}  // namespace

std::string X86Codegen::generateEntryPoint(
    const std::string& entryFunction
) {
    std::ostringstream out;

    out << ".text\n";
    out << ".global _start\n";
    out << "_start:\n";
    out << "    call " << entryFunction << "\n";
    out << "    movl %eax, %edi\n";
    out << "    movl $60, %eax\n";
    out << "    syscall\n";
    out << "\n";

    return out.str();
}

std::string X86Codegen::generate(
    const IRFunction& ir,
    const RegisterAllocation& allocation,
    const std::unordered_map<ValueId, LiveRange>& liveness
) {
    std::ostringstream out;

    // Spill slot N lives at -8*(N+1)(%rbp). 8-byte-aligned slots even
    // though every value here is 32-bit: simple arithmetic, and the
    // frame pointer makes the offset valid regardless of how much the
    // balanced pushq/popq churn from call-argument marshaling or
    // division scratch-saving transiently moves rsp elsewhere in the
    // body.
    auto spillAddress = [&](ValueId value) -> std::string {
        auto it = allocation.spillSlots.find(value);
        if (it == allocation.spillSlots.end()) {
            throw std::runtime_error(
                "Value has no spill slot"
            );
        }
        return "-" + std::to_string((it->second + 1) * 8) + "(%rbp)";
    };

    auto isSpilled = [&](ValueId value) -> bool {
        return allocation.registers.find(value) ==
               allocation.registers.end();
    };

    // Reads `value`: returns its real register operand directly, or
    // loads it from its spill slot into `scratch` first and returns
    // that. Callers pick distinct scratch registers (ebx/edi/r8d — all
    // permanently outside the register allocator's pool of
    // eax/ecx/edx/esi, and not held across instruction boundaries by
    // anything else) so two spilled operands in the same instruction
    // never clobber each other.
    auto loadRead = [&](ValueId value, const char* scratch) -> std::string {
        auto it = allocation.registers.find(value);
        if (it != allocation.registers.end()) {
            return "%" + it->second;
        }

        out << "    movl " << spillAddress(value) << ", %" << scratch << "\n";
        return std::string("%") + scratch;
    };

    // Returns where to compute a result for `value`: its real register,
    // or `scratch` if it's spilled. Pair with storeSpilled after the
    // computation to flush a spilled result back to memory.
    auto writeTarget = [&](ValueId value, const char* scratch) -> std::string {
        auto it = allocation.registers.find(value);
        if (it != allocation.registers.end()) {
            return "%" + it->second;
        }
        return std::string("%") + scratch;
    };

    auto storeIfSpilled = [&](ValueId value, const char* scratch) {
        if (isSpilled(value)) {
            out << "    movl %" << scratch << ", "
                << spillAddress(value) << "\n";
        }
    };

    // Physical pool registers holding a value that is both defined
    // strictly before `index` and still needed strictly after it —
    // i.e. a value that a call or division at `index` would otherwise
    // clobber. Spilled values are never included: they live in memory,
    // which a call or idivl can't touch, so they never need saving.
    // `exclude` is this instruction's own destination, which never
    // needs preserving (nothing depends on its old contents).
    auto liveAcross = [&](
        int index,
        ValueId exclude
    ) -> std::vector<std::pair<ValueId, std::string>> {

        std::vector<std::pair<ValueId, std::string>> result;

        for (const auto& [value, physReg] : allocation.registers) {
            if (value == exclude)
                continue;

            auto it = liveness.find(value);
            if (it == liveness.end())
                continue;

            if (it->second.start < index && it->second.end > index) {
                result.push_back({value, physReg});
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    };

    // Shared codegen for add/sub/imul. All three follow the same
    // 2-address pattern (move left into destination, then operate
    // destination against right) — which is unsafe whenever the
    // resolved destination location is the same as the resolved right
    // operand while the left operand sits elsewhere: the "move left
    // into destination" step would silently clobber right's value
    // before the operation reads it. When that happens, stage the
    // original right-hand value on the stack first and operate against
    // it there instead. This can only actually happen when neither
    // operand is spilled (spilled operands are loaded into dedicated
    // scratch registers that are never reused as the destination
    // scratch for a *different* still-needed value in the same
    // instruction), but the check is unconditional so it stays correct
    // regardless of exactly which values end up spilled.
    auto emitBinaryOp = [&](
        const char* mnemonic,
        ValueId destination,
        ValueId left,
        ValueId right
    ) {
        std::string leftLoc = loadRead(left, "ebx");
        std::string rightLoc = loadRead(right, "edi");
        std::string destLoc = writeTarget(destination, "r8d");

        if (destLoc == rightLoc && destLoc != leftLoc) {
            out << "    pushq %" << to64(rightLoc) << "\n";
            out << "    movl " << leftLoc << ", " << destLoc << "\n";
            out << "    " << mnemonic << "l (%rsp), " << destLoc << "\n";
            out << "    addq $8, %rsp\n";
        } else {
            if (destLoc != leftLoc) {
                out << "    movl " << leftLoc << ", " << destLoc << "\n";
            }
            out << "    " << mnemonic << "l " << rightLoc << ", " << destLoc << "\n";
        }

        storeIfSpilled(destination, "r8d");
    };

    // setcc only writes an 8-bit register; every register we ever use
    // (pool or scratch) has a directly addressable low-byte alias in
    // 64-bit mode.
    auto byteAlias = [](const std::string& reg) -> std::string {
        if (reg == "%eax") return "%al";
        if (reg == "%ecx") return "%cl";
        if (reg == "%edx") return "%dl";
        if (reg == "%esi") return "%sil";
        if (reg == "%ebx") return "%bl";
        if (reg == "%edi") return "%dil";
        if (reg == "%r8d") return "%r8b";
        throw std::runtime_error("No 8-bit alias for register: " + reg);
    };

    int spillBytes = allocation.spillSlotCount * 8;

    out << ".global " << ir.name << "\n";
    out << ir.name << ":\n";
    out << "    pushq %rbp\n";
    out << "    movq %rsp, %rbp\n";
    if (spillBytes > 0) {
        out << "    subq $" << spillBytes << ", %rsp\n";
    }

    // Unpack incoming parameters before the main instruction loop,
    // using the same stack-mediated technique as call-argument
    // marshaling and for the same reason: an earlier parameter's
    // destination can coincide with a *later* parameter's ABI source
    // register whenever the allocator determines the two don't
    // interfere (e.g. the first parameter is dead by the time the last
    // one is read). Writing destinations one at a time directly from
    // edi/esi/edx/ecx would then silently clobber a not-yet-read
    // argument. Pushing all of them first and popping each into its
    // real destination decouples "read the incoming values" from
    // "write the destinations" entirely, exactly as it does for calls.
    // By construction (IRLowerer::lower), the first ir.paramCount
    // instructions are exactly the ParamI32s, in order. Only the first
    // 4 arrive in registers; any beyond that were pushed onto the
    // stack by the caller (see the Call case below) and are read
    // directly at a fixed rbp-relative offset instead, with no clobber
    // hazard to resolve since each stack slot is independently
    // addressed.
    int registerParamCount = std::min(
        ir.paramCount, static_cast<int>(kAbiArgRegisters.size())
    );

    if (registerParamCount > 0) {
        for (int p = registerParamCount - 1; p >= 0; --p) {
            out << "    pushq %" << to64(kAbiArgRegisters[p]) << "\n";
        }

        for (int p = 0; p < registerParamCount; ++p) {
            ValueId dest = ir.instructions[p].destination;

            if (isSpilled(dest)) {
                out << "    popq " << spillAddress(dest) << "\n";
            } else {
                out << "    popq %"
                    << to64(allocation.registers.at(dest)) << "\n";
            }
        }
    }

    for (int i = 0; i < static_cast<int>(ir.instructions.size()); ++i) {
        const auto& inst = ir.instructions[i];

        switch (inst.opcode) {

            case OpCode::ConstI32: {
                std::string destLoc = writeTarget(inst.destination, "ebx");
                out << "    movl $" << inst.value << ", " << destLoc << "\n";
                storeIfSpilled(inst.destination, "ebx");
                break;
            }

            case OpCode::AddI32:
                emitBinaryOp("add", inst.destination, inst.left, inst.right);
                break;

            case OpCode::SubI32:
                emitBinaryOp("sub", inst.destination, inst.left, inst.right);
                break;

            case OpCode::MulI32:
                emitBinaryOp("imul", inst.destination, inst.left, inst.right);
                break;

            case OpCode::MoveI32: {
                std::string srcLoc = loadRead(inst.left, "ebx");
                std::string destLoc = writeTarget(inst.destination, "ebx");

                if (destLoc != srcLoc) {
                    out << "    movl " << srcLoc << ", " << destLoc << "\n";
                }

                storeIfSpilled(inst.destination, "ebx");
                break;
            }

            // Pointers are addresses, and this target is x86-64: they
            // are 64-bit values even though every other OGLang value is
            // 32-bit. A stack address (which is what AddressOfI32
            // always produces here, since address-taken variables are
            // forced into a stack slot) routinely lives well above the
            // 4 GiB boundary on a real 64-bit process, so computing or
            // spilling one through a 32-bit register/slot silently
            // truncates it into a bogus address — this was a real bug
            // caught by the very first pointer-aliasing test written
            // against this feature (immediate segfault), not by
            // inspection. Pointer-valued operands are therefore always
            // handled through their full 64-bit register form (leaq,
            // movq for spilling) here, distinct from the 32-bit
            // load/writeTarget/storeIfSpilled helpers everything else
            // uses.
            case OpCode::AddressOfI32: {
                bool spilled = isSpilled(inst.destination);
                std::string destReg64 =
                    spilled ? "rbx" : to64(allocation.registers.at(inst.destination));

                out << "    leaq " << spillAddress(inst.left)
                    << ", %" << destReg64 << "\n";

                if (spilled) {
                    out << "    movq %rbx, " << spillAddress(inst.destination) << "\n";
                }
                break;
            }

            case OpCode::LoadI32: {
                std::string pointerReg64;
                if (isSpilled(inst.left)) {
                    out << "    movq " << spillAddress(inst.left) << ", %rbx\n";
                    pointerReg64 = "rbx";
                } else {
                    pointerReg64 = to64(allocation.registers.at(inst.left));
                }

                std::string destLoc = writeTarget(inst.destination, "r8d");

                out << "    movl (%" << pointerReg64 << "), "
                    << destLoc << "\n";

                storeIfSpilled(inst.destination, "r8d");
                break;
            }

            case OpCode::StoreI32: {
                std::string pointerReg64;
                if (isSpilled(inst.left)) {
                    out << "    movq " << spillAddress(inst.left) << ", %rbx\n";
                    pointerReg64 = "rbx";
                } else {
                    pointerReg64 = to64(allocation.registers.at(inst.left));
                }

                std::string valueLoc = loadRead(inst.right, "edi");

                out << "    movl " << valueLoc << ", (%"
                    << pointerReg64 << ")\n";

                break;
            }

            case OpCode::PtrSubI32: {
                // `left`/`destination` are 64-bit pointer values;
                // `right` is a plain i32 byte offset that must be
                // sign-extended to 64 bits before the subtraction —
                // using a 32-bit sub here would be the same address-
                // truncating bug AddressOfI32/LoadI32/StoreI32 already
                // had to avoid.
                std::string pointerReg64;
                if (isSpilled(inst.left)) {
                    out << "    movq " << spillAddress(inst.left) << ", %rbx\n";
                    pointerReg64 = "rbx";
                } else {
                    pointerReg64 = to64(allocation.registers.at(inst.left));
                }

                std::string offsetLoc = loadRead(inst.right, "edi");
                out << "    movslq " << offsetLoc << ", %rdi\n";

                bool spilled = isSpilled(inst.destination);
                std::string destReg64 = spilled
                    ? "rbx"
                    : to64(allocation.registers.at(inst.destination));

                if (destReg64 != pointerReg64) {
                    out << "    movq %" << pointerReg64 << ", %"
                        << destReg64 << "\n";
                }

                out << "    subq %rdi, %" << destReg64 << "\n";

                if (spilled) {
                    out << "    movq %rbx, "
                        << spillAddress(inst.destination) << "\n";
                }

                break;
            }

            case OpCode::DivI32: {
                // idivl requires the dividend sign-extended across
                // edx:eax and forbids eax/edx as the divisor operand.
                // Both are clobbered unconditionally, so any other
                // register-allocated value still needed after this
                // instruction that currently occupies eax or edx must
                // be saved first.
                std::string rightLoc = loadRead(inst.right, "edi");
                std::string leftLoc = loadRead(inst.left, "ebx");

                auto saved = liveAcross(i, inst.destination);

                std::vector<std::pair<ValueId, std::string>> toSave;
                for (auto& [value, physReg] : saved) {
                    if (physReg == "eax" || physReg == "edx") {
                        toSave.push_back({value, physReg});
                    }
                }

                for (auto& [value, physReg] : toSave) {
                    (void)value;
                    out << "    pushq %" << to64(physReg) << "\n";
                }

                // Capture the divisor before eax/edx are overwritten,
                // in case it currently lives in one of them.
                out << "    movl " << rightLoc << ", %ecx\n";
                out << "    movl " << leftLoc << ", %eax\n";
                out << "    cdq\n";
                out << "    idivl %ecx\n";

                std::string destLoc = writeTarget(inst.destination, "ebx");
                if (destLoc != "%eax") {
                    out << "    movl %eax, " << destLoc << "\n";
                }
                storeIfSpilled(inst.destination, "ebx");

                for (auto it = toSave.rbegin(); it != toSave.rend(); ++it) {
                    if (!isSpilled(inst.destination) &&
                        destLoc == "%" + it->second) {
                        // The destination reclaimed this physical
                        // register; its old contents are no longer
                        // needed by anyone (interference analysis
                        // guarantees that), so discard instead of
                        // clobbering the fresh result.
                        out << "    addq $8, %rsp\n";
                    } else {
                        out << "    popq %" << to64(it->second) << "\n";
                    }
                }

                break;
            }

            case OpCode::CmpEqI32:
            case OpCode::CmpNeI32:
            case OpCode::CmpGtI32:
            case OpCode::CmpLtI32:
            case OpCode::CmpGeI32:
            case OpCode::CmpLeI32: {
                std::string leftLoc = loadRead(inst.left, "ebx");
                std::string rightLoc = loadRead(inst.right, "edi");

                out << "    cmpl " << rightLoc << ", " << leftLoc << "\n";

                const char* setcc = nullptr;
                switch (inst.opcode) {
                    case OpCode::CmpEqI32: setcc = "sete"; break;
                    case OpCode::CmpNeI32: setcc = "setne"; break;
                    case OpCode::CmpGtI32: setcc = "setg"; break;
                    case OpCode::CmpLtI32: setcc = "setl"; break;
                    case OpCode::CmpGeI32: setcc = "setge"; break;
                    case OpCode::CmpLeI32: setcc = "setle"; break;
                    default: break;
                }

                std::string destLoc = writeTarget(inst.destination, "r8d");
                std::string destByte = byteAlias(destLoc);

                out << "    " << setcc << " " << destByte << "\n";
                out << "    movzbl " << destByte << ", " << destLoc << "\n";

                storeIfSpilled(inst.destination, "r8d");

                break;
            }

            case OpCode::ParamI32:
                if (inst.value < 0) {
                    throw std::runtime_error(
                        "Invalid parameter index"
                    );
                }

                if (inst.value < static_cast<int>(kAbiArgRegisters.size())) {
                    // Already unpacked by the stack-mediated prologue
                    // sequence above, before this loop starts.
                    break;
                }

                {
                    // Stack-passed parameter: the caller pushed it at
                    // a fixed offset relative to the return address
                    // (see the Call case's step 4), so 16(%rbp) is
                    // argument index kAbiArgRegisters.size(), each
                    // subsequent one 8 bytes further — 16 accounts for
                    // the saved rbp (8) and the return address (8)
                    // this function's own prologue and `call` pushed.
                    int stackIndex =
                        inst.value - static_cast<int>(kAbiArgRegisters.size());
                    std::string src =
                        std::to_string(16 + stackIndex * 8) + "(%rbp)";

                    std::string destLoc = writeTarget(inst.destination, "ebx");
                    out << "    movl " << src << ", " << destLoc << "\n";
                    storeIfSpilled(inst.destination, "ebx");
                }

                break;

            case OpCode::Call: {
                int registerArgCount = std::min(
                    static_cast<int>(inst.args.size()),
                    static_cast<int>(kAbiArgRegisters.size())
                );
                int stackArgCount =
                    static_cast<int>(inst.args.size()) - registerArgCount;

                auto pushArgValue = [&](ValueId arg) {
                    if (isSpilled(arg)) {
                        out << "    pushq " << spillAddress(arg) << "\n";
                    } else {
                        out << "    pushq %"
                            << to64(allocation.registers.at(arg)) << "\n";
                    }
                };

                auto saved = liveAcross(i, inst.destination);

                // 1. Preserve every pool register holding a value the
                //    callee might clobber and that is still needed
                //    after the call. Spilled values need no saving —
                //    they already live in memory the callee can't
                //    touch.
                for (auto& [value, physReg] : saved) {
                    (void)value;
                    out << "    pushq %" << to64(physReg) << "\n";
                }

                // 2. Push any arguments beyond the 4 ABI registers
                //    first, in descending index order, reading each
                //    one from its *original* register or spill slot —
                //    crucially, before step 3 below pops anything into
                //    edx/ecx/esi, which overlap the general register
                //    pool and could otherwise silently clobber a
                //    stack-bound argument that just hadn't been read
                //    yet (this exact bug shipped once and was caught
                //    by tests/programs/spill_stress.og-style stress
                //    testing, not by inspection). Descending order
                //    means the lowest-indexed stack argument (arg 4)
                //    ends up pushed last and therefore closest to the
                //    return address once `call` pushes it — i.e. at a
                //    fixed, predictable offset (16(%rbp)) in the
                //    callee, with each subsequent one 8 bytes further.
                //    See ParamI32 above for the matching read side.
                for (int argIndex = static_cast<int>(inst.args.size()) - 1;
                     argIndex >= registerArgCount;
                     --argIndex) {
                    pushArgValue(inst.args[argIndex]);
                }

                // 3. Push the first (up to 4) argument values in
                //    left-to-right order — still reading every one
                //    from its original location, since nothing has
                //    been popped yet. A spilled argument is pushed
                //    directly from its spill slot (the stack-mediated
                //    marshaling here already goes through memory, so
                //    there's nothing to gain by loading it into a
                //    register first; the garbage upper 32 bits carried
                //    along are never read back, since everything here
                //    only ever reads the low 32-bit view of a popped
                //    register).
                for (int argIndex = 0; argIndex < registerArgCount; ++argIndex) {
                    pushArgValue(inst.args[argIndex]);
                }

                // 4. Now, and only now, pop the register arguments off
                //    into the ABI registers, in reverse — which also
                //    resolves any overlap between an argument's source
                //    register and another argument's target register
                //    (or a saved register) purely through memory,
                //    without a parallel-move algorithm. This leaves
                //    the stack arguments from step 2 undisturbed
                //    underneath, already in the right position for the
                //    callee.
                for (int argIndex = registerArgCount - 1;
                     argIndex >= 0;
                     --argIndex) {
                    out << "    popq %"
                        << to64(kAbiArgRegisters[argIndex]) << "\n";
                }

                out << "    call " << inst.label << "\n";

                if (stackArgCount > 0) {
                    out << "    addq $" << (stackArgCount * 8)
                        << ", %rsp\n";
                }

                std::string destLoc = writeTarget(inst.destination, "ebx");
                if (destLoc != "%eax") {
                    out << "    movl %eax, " << destLoc << "\n";
                }
                storeIfSpilled(inst.destination, "ebx");

                // 4. Restore, in reverse push order.
                for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
                    if (!isSpilled(inst.destination) &&
                        destLoc == "%" + it->second) {
                        out << "    addq $8, %rsp\n";
                    } else {
                        out << "    popq %" << to64(it->second) << "\n";
                    }
                }

                break;
            }

            case OpCode::BoundsCheckI32: {
                // Unsigned comparison: index >= size catches both a
                // too-large index and a negative one (which, read as
                // unsigned, is huge) with a single check. `i` (this
                // instruction's own position) makes a unique label
                // without needing a separate counter threaded through
                // codegen.
                std::string indexLoc = loadRead(inst.left, "ebx");
                std::string okLabel = ".Lboundsok" + std::to_string(i);

                out << "    cmpl $" << inst.value << ", " << indexLoc << "\n";
                out << "    jb " << okLabel << "\n";

                // Out of bounds: exit(101) rather than silently
                // computing and using an out-of-range address. 101 is
                // a distinct, documented status specifically for this
                // trap (60 = SYS_exit, edi = status code — the same
                // raw syscall convention generateEntryPoint uses).
                out << "    movl $60, %eax\n";
                out << "    movl $101, %edi\n";
                out << "    syscall\n";

                out << okLabel << ":\n";
                break;
            }

            case OpCode::Label:
                out << inst.label << ":\n";
                break;

            case OpCode::Jump:
                out << "    jmp " << inst.label << "\n";
                break;

            case OpCode::JumpIfZero: {
                std::string condLoc = loadRead(inst.left, "ebx");
                out << "    testl " << condLoc << ", " << condLoc << "\n";
                out << "    jz " << inst.label << "\n";
                break;
            }

            case OpCode::ReturnI32: {
                std::string valueLoc = loadRead(inst.left, "ebx");
                if (valueLoc != "%eax") {
                    out << "    movl " << valueLoc << ", %eax\n";
                }

                out << "    leave\n";
                out << "    ret\n";
                break;
            }
        }
    }

    out << "\n";

    return out.str();
}
