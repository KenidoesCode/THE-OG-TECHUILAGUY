#include "x86_64.hpp"

#include <stdexcept>
#include <string>

std::string X86Codegen::generate(const IRFunction& ir) {
    std::string assembly;

    assembly += ".text\n";
    assembly += ".global main\n";
    assembly += "main:\n";

    for (const auto& instruction : ir.instructions) {

        switch (instruction.opcode) {

            case OpCode::ConstI32:

                if (instruction.destination == "x") {
                    assembly +=
                        "    movl $" +
                        std::to_string(instruction.value) +
                        ", %eax\n";

                } else if (instruction.destination == "y") {
                    assembly +=
                        "    movl $" +
                        std::to_string(instruction.value) +
                        ", %ecx\n";

                } else {
                    throw std::runtime_error(
                        "Unknown variable: " +
                        instruction.destination
                    );
                }

                break;

            case OpCode::AddI32:
                assembly += "    addl %ecx, %eax\n";
                break;

            case OpCode::ReturnI32:
                assembly += "    ret\n";
                break;
        }
    }

    return assembly;
}
