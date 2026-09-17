#include "register_allocator.hpp"

#include <unordered_set>
#include <vector>

RegisterAllocation RegisterAllocator::allocate(
    const InterferenceGraph& graph
) {
    const std::vector<std::string> registers = {
        "eax", "ecx", "edx", "esi"
    };
    const int K = static_cast<int>(registers.size());

    RegisterAllocation result;

    // Working copy whose adjacency sets shrink as nodes are simplified
    // away, so degree checks reflect only the neighbors still in play.
    std::unordered_map<ValueId, std::unordered_set<ValueId>> working(
        graph.begin(), graph.end()
    );

    struct StackEntry {
        ValueId value;
    };
    std::vector<StackEntry> stack;
    stack.reserve(working.size());

    while (!working.empty()) {
        ValueId chosen = working.begin()->first;
        bool foundSimplifiable = false;

        for (const auto& [value, neighbors] : working) {
            if (static_cast<int>(neighbors.size()) < K) {
                chosen = value;
                foundSimplifiable = true;
                break;
            }
        }

        if (!foundSimplifiable) {
            // No node is safely colorable right now. Optimistically
            // remove the highest-degree node as a *potential* spill —
            // it only becomes an actual spill in the select phase
            // below if coloring genuinely fails for it.
            int highestDegree = -1;
            for (const auto& [value, neighbors] : working) {
                if (static_cast<int>(neighbors.size()) > highestDegree) {
                    highestDegree = static_cast<int>(neighbors.size());
                    chosen = value;
                }
            }
        }

        for (ValueId neighbor : working[chosen]) {
            auto it = working.find(neighbor);
            if (it != working.end()) {
                it->second.erase(chosen);
            }
        }
        working.erase(chosen);

        stack.push_back({chosen});
    }

    // Select phase: assign colors in reverse simplify order, using the
    // *original* graph's full neighbor set (working's sets have been
    // destructively shrunk to nothing by now).
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        ValueId value = it->value;

        std::vector<bool> used(K, false);

        auto neighborsIt = graph.find(value);
        if (neighborsIt != graph.end()) {
            for (ValueId neighbor : neighborsIt->second) {
                auto regIt = result.registers.find(neighbor);
                if (regIt == result.registers.end())
                    continue;

                for (int i = 0; i < K; ++i) {
                    if (registers[i] == regIt->second) {
                        used[i] = true;
                    }
                }
            }
        }

        int chosenReg = -1;
        for (int i = 0; i < K; ++i) {
            if (!used[i]) {
                chosenReg = i;
                break;
            }
        }

        if (chosenReg >= 0) {
            result.registers[value] = registers[chosenReg];
        } else {
            result.spillSlots[value] = result.spillSlotCount++;
        }
    }

    return result;
}
