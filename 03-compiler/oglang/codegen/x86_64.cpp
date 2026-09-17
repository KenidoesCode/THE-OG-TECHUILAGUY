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
    const std::unordered_map<ValueId, std::string>& allocation,
    const std::unordered_map<ValueId, LiveRange>& liveness
) {
    std::ostringstream out;

    auto reg = [&](ValueId value) -> std::string {
        auto it = allocation.find(value);

        if (it == allocation.end()) {
            throw std::runtime_error(
                "No register allocated for value"
            );
        }

        return "%" + it->second;
    };

    // Physical pool registers holding a value that is both defined
    // strictly before `index` and still needed strictly after it —
    // i.e. a value that a call or division at `index` would otherwise
    // clobber. `exclude` is this instruction's own destination, which
    // never needs preserving (nothing depends on its old contents).
    auto liveAcross = [&](
        int index,
        ValueId exclude
    ) -> std::vector<std::pair<ValueId, std::string>> {

        std::vector<std::pair<ValueId, std::string>> result;

        for (const auto& [value, physReg] : allocation) {
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
    // register allocator happens to give the destination the same
    // physical register as the *right* operand while the left operand
    // sits elsewhere: the "move left into destination" step would
    // silently clobber right's value before the operation reads it.
    // When that happens, stage the original right-hand value on the
    // stack first and operate against it there instead.
    auto emitBinaryOp = [&](
        const char* mnemonic,
        ValueId destination,
        ValueId left,
        ValueId right
    ) {
        std::string destReg = reg(destination);
        std::string leftReg = reg(left);
        std::string rightReg = reg(right);

        if (destReg == rightReg && destReg != leftReg) {
            out << "    pushq %" << to64(rightReg) << "\n";
            out << "    movl " << leftReg << ", " << destReg << "\n";
            out << "    " << mnemonic << "l (%rsp), " << destReg << "\n";
            out << "    addq $8, %rsp\n";
            return;
        }

        if (destReg != leftReg) {
            out << "    movl " << leftReg << ", " << destReg << "\n";
        }

        out << "    " << mnemonic << "l " << rightReg << ", " << destReg << "\n";
    };

    out << ".global " << ir.name << "\n";
    out << ir.name << ":\n";

    for (int i = 0; i < static_cast<int>(ir.instructions.size()); ++i) {
        const auto& inst = ir.instructions[i];

        switch (inst.opcode) {

            case OpCode::ConstI32:
                out << "    movl $"
                    << inst.value
                    << ", "
                    << reg(inst.destination)
                    << "\n";
                break;

            case OpCode::AddI32:
                emitBinaryOp("add", inst.destination, inst.left, inst.right);
                break;

            case OpCode::SubI32:
                emitBinaryOp("sub", inst.destination, inst.left, inst.right);
                break;

            case OpCode::MulI32:
                emitBinaryOp("imul", inst.destination, inst.left, inst.right);
                break;

            case OpCode::DivI32: {
                // idivl requires the dividend sign-extended across
                // edx:eax and forbids eax/edx as the divisor operand.
                // Both are clobbered unconditionally, so any other
                // value currently held in eax or edx that is still
                // needed after this instruction must be saved first.
                std::string leftReg = reg(inst.left);
                std::string rightReg = reg(inst.right);

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
                out << "    movl " << rightReg << ", %ecx\n";
                out << "    movl " << leftReg << ", %eax\n";
                out << "    cdq\n";
                out << "    idivl %ecx\n";

                if (reg(inst.destination) != "%eax") {
                    out << "    movl %eax, "
                        << reg(inst.destination) << "\n";
                }

                for (auto it = toSave.rbegin(); it != toSave.rend(); ++it) {
                    if (reg(inst.destination) == "%" + it->second) {
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
                out << "    cmpl "
                    << reg(inst.right)
                    << ", "
                    << reg(inst.left)
                    << "\n";

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

                std::string destReg = reg(inst.destination);

                // setcc only writes an 8-bit register. In 64-bit mode
                // every pool register (eax/ecx/edx/esi) has a directly
                // addressable low-byte alias.
                std::string destByte;
                if (destReg == "%eax") destByte = "%al";
                else if (destReg == "%ecx") destByte = "%cl";
                else if (destReg == "%edx") destByte = "%dl";
                else if (destReg == "%esi") destByte = "%sil";
                else throw std::runtime_error(
                    "No 8-bit alias for register: " + destReg
                );

                out << "    " << setcc << " " << destByte << "\n";
                out << "    movzbl " << destByte << ", "
                    << destReg << "\n";

                break;
            }

            case OpCode::ParamI32: {
                if (inst.value < 0 ||
                    inst.value >= static_cast<int>(kAbiArgRegisters.size())) {
                    throw std::runtime_error(
                        "OGLang currently supports at most " +
                        std::to_string(kAbiArgRegisters.size()) +
                        " integer parameters"
                    );
                }

                std::string abiReg = kAbiArgRegisters[inst.value];

                if (reg(inst.destination) != "%" + abiReg) {
                    out << "    movl %" << abiReg << ", "
                        << reg(inst.destination) << "\n";
                }

                break;
            }

            case OpCode::Call: {
                if (inst.args.size() > kAbiArgRegisters.size()) {
                    throw std::runtime_error(
                        "OGLang currently supports at most " +
                        std::to_string(kAbiArgRegisters.size()) +
                        " call arguments"
                    );
                }

                auto saved = liveAcross(i, inst.destination);

                // 1. Preserve every pool register holding a value the
                //    callee might clobber and that is still needed
                //    after the call.
                for (auto& [value, physReg] : saved) {
                    (void)value;
                    out << "    pushq %" << to64(physReg) << "\n";
                }

                // 2. Push argument values (read directly from their
                //    current registers, unaffected by step 1's
                //    pushes) in left-to-right order.
                for (ValueId arg : inst.args) {
                    out << "    pushq %" << to64(reg(arg)) << "\n";
                }

                // 3. Pop them into the ABI registers in reverse, which
                //    resolves any overlap between an argument's source
                //    register and another argument's target register
                //    (or a saved register) purely through memory,
                //    without a parallel-move algorithm.
                for (int argIndex = static_cast<int>(inst.args.size()) - 1;
                     argIndex >= 0;
                     --argIndex) {
                    out << "    popq %"
                        << to64(kAbiArgRegisters[argIndex]) << "\n";
                }

                out << "    call " << inst.label << "\n";

                if (reg(inst.destination) != "%eax") {
                    out << "    movl %eax, "
                        << reg(inst.destination) << "\n";
                }

                // 4. Restore, in reverse push order.
                for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
                    if (reg(inst.destination) == "%" + it->second) {
                        out << "    addq $8, %rsp\n";
                    } else {
                        out << "    popq %" << to64(it->second) << "\n";
                    }
                }

                break;
            }

            case OpCode::Label:
                out << inst.label << ":\n";
                break;

            case OpCode::Jump:
                out << "    jmp " << inst.label << "\n";
                break;

            case OpCode::JumpIfZero:
                out << "    testl "
                    << reg(inst.left) << ", " << reg(inst.left) << "\n";
                out << "    jz " << inst.label << "\n";
                break;

            case OpCode::ReturnI32:
                if (reg(inst.left) != "%eax") {
                    out << "    movl "
                        << reg(inst.left)
                        << ", %eax\n";
                }

                out << "    ret\n";
                break;
        }
    }

    out << "\n";

    return out.str();
}
