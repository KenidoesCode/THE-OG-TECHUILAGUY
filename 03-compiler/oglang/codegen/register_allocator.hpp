#pragma once

#include "../analysis/interference.hpp"
#include <string>
#include <unordered_map>

class RegisterAllocator {
public:
    std::unordered_map<ValueId, std::string>
    allocate(const InterferenceGraph& graph);
};
