#include <iostream>
#include "cpu.hpp"

int main() {
    CPU cpu;
    cpu.reset();

    std::cout << "Techuilaguy CPU initialized\n";
    std::cout << "PC = " << cpu.pc << '\n';

    return 0;
}
