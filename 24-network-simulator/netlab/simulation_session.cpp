#include "simulation_session.hpp"

#include <algorithm>

namespace netlab {

SimulationSession::SimulationSession(std::vector<SimulationEvent> events) : events(std::move(events)) {}

const SimulationEvent* SimulationSession::stepForward() {
    if (cursor >= events.size()) return nullptr;
    cursor += 1;
    return &events[cursor - 1];
}

const SimulationEvent* SimulationSession::stepBackward() {
    if (cursor == 0) return nullptr;
    cursor -= 1;
    if (cursor == 0) return nullptr;
    return &events[cursor - 1];
}

void SimulationSession::reset() {
    cursor = 0;
}

void SimulationSession::jumpTo(size_t index) {
    cursor = std::min(index, events.size());
}

std::vector<SimulationEvent> SimulationSession::eventsSoFar() const {
    return std::vector<SimulationEvent>(events.begin(), events.begin() + static_cast<long>(cursor));
}

}  // namespace netlab
