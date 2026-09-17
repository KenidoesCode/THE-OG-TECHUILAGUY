#include "liveness.hpp"

#include <algorithm>

std::unordered_map<ValueId, LiveRange>
LivenessAnalyzer::analyze(const IRFunction& function) {

    std::unordered_map<ValueId, LiveRange> ranges;

    for (int i = 0; i < (int)function.instructions.size(); ++i) {

        const auto& inst = function.instructions[i];

        auto touch = [&](ValueId value) {
            if (value < 0)
                return;

            auto it = ranges.find(value);

            if (it == ranges.end()) {
                ranges[value] = {i, i};
            } else {
                it->second.end =
                    std::max(it->second.end, i);
            }
        };

        touch(inst.destination);
        touch(inst.left);
        touch(inst.right);

        for (ValueId arg : inst.args) {
            touch(arg);
        }
    }

    return ranges;
}
