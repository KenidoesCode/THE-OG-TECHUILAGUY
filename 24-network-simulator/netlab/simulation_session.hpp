#pragma once

#include "ip.hpp"

// A cursor over an already-computed, immutable simulation timeline —
// the step/pause/rewind/replay core a future UI (of any kind) would
// drive. UI-independent, no live/streaming simulation. See
// docs/ADR/0029-netlab-packet-inspector-and-timeline-session.md.

namespace netlab {

class SimulationSession {
public:
    explicit SimulationSession(std::vector<SimulationEvent> events);

    size_t eventCount() const { return events.size(); }

    // How many events have been "revealed" so far: 0 = nothing shown
    // yet, eventCount() = the full timeline has played through.
    size_t currentIndex() const { return cursor; }

    bool atStart() const { return cursor == 0; }
    bool atEnd() const { return cursor == events.size(); }

    // Reveals the next event and returns a pointer to it, or nullptr
    // (moving nothing) if already at the end.
    const SimulationEvent* stepForward();

    // Hides the most recently revealed event and returns a pointer to
    // whatever is now the current (last-revealed) event, or nullptr if
    // already at the start (nothing left revealed).
    const SimulationEvent* stepBackward();

    void reset();

    // Moves directly to `index`, clamped to [0, eventCount()].
    void jumpTo(size_t index);

    // The events revealed as of the CURRENT cursor position — the
    // "replay so far" a timeline view would render.
    std::vector<SimulationEvent> eventsSoFar() const;

private:
    std::vector<SimulationEvent> events;
    size_t cursor = 0;
};

}  // namespace netlab
