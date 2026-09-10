#pragma once

#include "../ir/ir.hpp"
#include <unordered_map>

struct LiveRange {
    int start;
    int end;
};

class LivenessAnalyzer {
public:
    std::unordered_map<ValueId, LiveRange>
    analyze(const IRFunction& function);
};
