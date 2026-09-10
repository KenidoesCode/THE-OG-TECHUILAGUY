#pragma once

#include "liveness.hpp"
#include <unordered_map>
#include <unordered_set>

using InterferenceGraph =
    std::unordered_map<ValueId, std::unordered_set<ValueId>>;

class InterferenceAnalyzer {
public:
    InterferenceGraph build(
        const std::unordered_map<ValueId, LiveRange>& ranges
    );
};
