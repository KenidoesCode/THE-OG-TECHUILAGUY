#include "cpu.hpp"

void CPU::reset() {
    for (auto &reg : registers)
        reg = 0;

    pc = 0;
}
