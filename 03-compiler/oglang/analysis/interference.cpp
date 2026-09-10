#include "interference.hpp"

InterferenceGraph InterferenceAnalyzer::build(
    const std::unordered_map<ValueId, LiveRange>& ranges) {

    InterferenceGraph graph;

    for (const auto& [a, rangeA] : ranges) {
        graph[a];

        for (const auto& [b, rangeB] : ranges) {

            if (a >= b)
                continue;

            bool overlap =
                rangeA.start < rangeB.end &&
                rangeB.start < rangeA.end;

            if (overlap) {
                graph[a].insert(b);
                graph[b].insert(a);
            }
        }
    }

    return graph;
}
