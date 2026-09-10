#include "x86_64.hpp"
#include <sstream>

std::string X86Codegen::generate(const IRFunction& function) {
    std::ostringstream out;

    out << ".text\n";
    out << ".global main\n";
    out << "main:\n";

    for (const auto& instruction : function.instructions) {
        if (instruction.opcode == OpCode::ReturnI32) {
            out << "    movl $" << instruction.operand << ", %eax\n";
            out << "    ret\n";
        }
    }

    return out.str();
}
