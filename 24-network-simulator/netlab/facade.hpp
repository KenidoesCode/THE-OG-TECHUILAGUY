#pragma once

#include "mission.hpp"
#include "packet_inspector.hpp"
#include "simulation_session.hpp"

#include <optional>

// A single, coherent API surface consolidating ADR 0022/0028/0029's
// already-existing, already-tested pieces — designed to be the
// boundary a future WebAssembly binding would wrap. No new networking
// logic lives here; every method delegates directly to something real
// that was already tested elsewhere. See
// docs/ADR/0030-netlab-browser-foundation-facade.md for exactly what
// this is (a pre-WASM API consolidation) and is not (a WASM build or
// a browser UI — neither exists yet; this environment has no
// Emscripten toolchain).

namespace netlab {

class NetLabFacade {
public:
    NetLabFacade() = default;

    // Builds the project's own canonical example — PC1 -> Switch ->
    // Router -> Switch -> PC2, with the same addressing ADR 0028's
    // tests already use — ready to run.
    static NetLabFacade buildReferenceScenario();

    Topology& topology() { return topo; }
    const Topology& topology() const { return topo; }

    // Runs a real ping (ADR 0028's sendIcmpEcho) and immediately
    // builds a fresh SimulationSession over the resulting timeline,
    // replacing any previous run's session.
    bool runPing(const std::string& fromHostId, uint32_t destIp);

    bool lastRunDelivered() const { return lastResult.delivered; }
    const std::string& lastFailureReason() const { return lastResult.failureReason; }

    // Timeline/session access — delegates directly to SimulationSession.
    size_t eventCount() const;
    size_t currentIndex() const;
    bool hasSession() const { return session.has_value(); }
    bool atStart() const;
    bool atEnd() const;
    bool stepForward();
    bool stepBackward();
    void resetSession();
    void jumpTo(size_t index);

    // The description of the most recently revealed event, or an
    // empty string if nothing has been revealed yet.
    std::string currentEventDescription() const;

    // Decodes the most recently revealed event's real frame bytes
    // (see packet_inspector.hpp) — DecodedFrame{parsedSuccessfully:
    // false} if there is no current event or it carries no frame
    // bytes (a pure narrative event).
    DecodedFrame currentEventDecoded() const;

    MissionResult runConnectTwoNetworksMission(const std::string& hostAId, const std::string& hostBId);

private:
    Topology topo;
    PingResult lastResult;
    std::optional<SimulationSession> session;
};

}  // namespace netlab
