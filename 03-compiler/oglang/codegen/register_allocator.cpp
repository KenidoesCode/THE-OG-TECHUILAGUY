#include "register_allocator.hpp"

#include <stdexcept>
#include <vector>

std::unordered_map<ValueId, std::string>
RegisterAllocator::allocate(const InterferenceGraph& graph) {

    const std::vector<std::string> registers = {
        "eax",
        "ecx",
        "edx",
        "esi"
    };

    std::unordered_map<ValueId, std::string> allocation;

    for (const auto& [value, neighbors] : graph) {

        for (const auto& reg : registers) {

            bool available = true;

            for (ValueId neighbor : neighbors) {
                auto it = allocation.find(neighbor);

                if (it != allocation.end() &&
                    it->second == reg) {
                    available = false;
                    break;
                }
            }

            if (available) {
                allocation[value] = reg;
                break;
            }
        }

        if (!allocation.contains(value)) {
            throw std::runtime_error(
                "Register allocation failed"
            );
        }
    }

    return allocation;
}
