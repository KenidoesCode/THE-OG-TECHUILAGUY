#pragma once

#include <cstdint>

class CPU {
public:
    uint64_t registers[8]{};
    uint64_t pc = 0;

    void reset();
};
