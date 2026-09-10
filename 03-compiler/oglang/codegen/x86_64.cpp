#include "x86_64.hpp"

#include <sstream>
#include <stdexcept>

std::string X86Codegen::generate(
    const IRFunction& ir,
    const std::unordered_map<int, std::string>& allocation
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

    out << ".text\n";
    out << ".global " << ir.name << "\n";
    out << ir.name << ":\n";

    for (const auto& inst : ir.instructions) {
        switch (inst.opcode) {

            case OpCode::ConstI32:
                out << "    movl $"
                    << inst.value
                    << ", "
                    << reg(inst.destination)
                    << "\n";
                break;

            case OpCode::AddI32:
                if (reg(inst.destination) != reg(inst.left)) {
                    out << "    movl "
                        << reg(inst.left)
                        << ", "
                        << reg(inst.destination)
                        << "\n";
                }

                out << "    addl "
                    << reg(inst.right)
                    << ", "
                    << reg(inst.destination)
                    << "\n";
                break;

            case OpCode::SubI32:
                if (reg(inst.destination) != reg(inst.left)) {
                    out << "    movl "
                        << reg(inst.left)
                        << ", "
                        << reg(inst.destination)
                        << "\n";
                }

                out << "    subl "
                    << reg(inst.right)
                    << ", "
                    << reg(inst.destination)
                    << "\n";
                break;

            case OpCode::MulI32:
                if (reg(inst.destination) != reg(inst.left)) {
                    out << "    movl "
                        << reg(inst.left)
                        << ", "
                        << reg(inst.destination)
                        << "\n";
                }

                out << "    imull "
                    << reg(inst.right)
                    << ", "
                    << reg(inst.destination)
                    << "\n";
                break;

            case OpCode::DivI32:
                throw std::runtime_error(
                    "i32 division requires fixed-register constraints"
                );

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

    return out.str();
}
