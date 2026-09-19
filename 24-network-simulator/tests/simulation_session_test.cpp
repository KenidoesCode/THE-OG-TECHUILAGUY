// Real assertion-based tests for NetLab's timeline session
// (netlab/simulation_session.cpp) — stepping over a real, multi-event
// timeline captured from an actual sendIcmpEcho run across a router,
// not a synthetic fixture. See
// docs/ADR/0029-netlab-packet-inspector-and-timeline-session.md.

#include "../netlab/mission.hpp"
#include "../netlab/simulation_session.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

net::MacAddress mac(uint8_t last) {
    return net::MacAddress{{0x02, 0x00, 0x00, 0x00, 0x00, last}};
}

const uint32_t SUBNET_MASK_24 = netlab::makeIpv4(255, 255, 255, 0);

std::vector<netlab::SimulationEvent> buildRealCrossRouterTimeline() {
    netlab::Topology topo;
    uint32_t gatewayA = netlab::makeIpv4(10, 0, 1, 1);
    uint32_t gatewayB = netlab::makeIpv4(10, 0, 2, 1);
    uint32_t pc2Ip = netlab::makeIpv4(10, 0, 2, 10);

    netlab::Node pc1{"PC1", netlab::NodeKind::Host, mac(1)};
    pc1.ipAddress = netlab::makeIpv4(10, 0, 1, 10);
    pc1.subnetMask = SUBNET_MASK_24;
    pc1.defaultGatewayIp = gatewayA;
    netlab::Node pc2{"PC2", netlab::NodeKind::Host, mac(2)};
    pc2.ipAddress = pc2Ip;
    pc2.subnetMask = SUBNET_MASK_24;
    pc2.defaultGatewayIp = gatewayB;

    topo.addNode(pc1);
    topo.addNode(pc2);
    topo.addNode(netlab::Node{"SW1", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"SW2", netlab::NodeKind::Switch, {}});
    topo.addNode(netlab::Node{"R1", netlab::NodeKind::Router, {}});
    topo.addLink("PC1", "SW1");
    topo.addLink("SW1", "R1");
    topo.addLink("R1", "SW2");
    topo.addLink("SW2", "PC2");
    topo.addRouterInterface("R1", "SW1", mac(101), gatewayA, SUBNET_MASK_24);
    topo.addRouterInterface("R1", "SW2", mac(102), gatewayB, SUBNET_MASK_24);

    netlab::PingResult result = netlab::sendIcmpEcho(topo, "PC1", pc2Ip);
    return result.timeline;
}

}  // namespace

void testStepForwardRevealsEventsInOrder() {
    std::vector<netlab::SimulationEvent> events = buildRealCrossRouterTimeline();
    check(events.size() >= 4, "simulation-session test setup: the real cross-router ping produced a multi-event timeline");

    netlab::SimulationSession session(events);
    check(session.atStart() && session.currentIndex() == 0, "simulation-session: a fresh session starts before the first event");

    for (size_t i = 0; i < events.size(); ++i) {
        const netlab::SimulationEvent* stepped = session.stepForward();
        check(stepped != nullptr && stepped->description == events[i].description,
              "simulation-session: stepForward() reveals events in exactly the recorded order");
    }
    check(session.atEnd(), "simulation-session: after stepping through every event, the session reports atEnd()");
    check(session.stepForward() == nullptr, "simulation-session: stepping forward past the end returns nullptr and does not crash");
}

void testStepBackwardReversesInOrder() {
    std::vector<netlab::SimulationEvent> events = buildRealCrossRouterTimeline();
    netlab::SimulationSession session(events);
    for (size_t i = 0; i < events.size(); ++i) session.stepForward();

    // After N stepForward calls (cursor == N, "current" == events[N-1]),
    // each stepBackward() hides the current event and returns the new
    // current one — so the first call returns events[N-2], the next
    // events[N-3], ..., down to events[0], after which the cursor is
    // back at the start (nothing left revealed).
    for (long expectedIndex = static_cast<long>(events.size()) - 2; expectedIndex >= 0; --expectedIndex) {
        const netlab::SimulationEvent* current = session.stepBackward();
        check(current != nullptr && current->description == events[static_cast<size_t>(expectedIndex)].description,
              "simulation-session: stepBackward() reveals the correct prior event at each step");
    }
    // The loop above walks back to events[0] still revealed (cursor ==
    // 1) — one more stepBackward() hides that last event too, which is
    // the actual transition to atStart() (cursor == 0).
    check(session.currentIndex() == 1, "simulation-session: after reversing through every event, exactly the first one remains revealed");
    check(session.stepBackward() == nullptr, "simulation-session: hiding the final remaining event returns nullptr (nothing left to show as \"current\")");
    check(session.atStart(), "simulation-session: after hiding every event, the session reports atStart()");
    check(session.stepBackward() == nullptr, "simulation-session: stepping backward past the start returns nullptr and does not crash");
}

void testJumpToAndReset() {
    std::vector<netlab::SimulationEvent> events = buildRealCrossRouterTimeline();
    netlab::SimulationSession session(events);

    session.jumpTo(2);
    check(session.currentIndex() == 2, "simulation-session: jumpTo() moves the cursor to the exact requested position");
    check(session.eventsSoFar().size() == 2, "simulation-session: eventsSoFar() reflects exactly the events revealed as of the current cursor");

    session.jumpTo(events.size() + 100);
    check(session.currentIndex() == events.size(), "simulation-session: jumpTo() clamps a too-large index to the real event count");

    session.reset();
    check(session.atStart(), "simulation-session: reset() returns to the start");
}

void testReplayingTheIdenticalSimulationProducesIdenticalSteppedEvents() {
    std::vector<netlab::SimulationEvent> eventsA = buildRealCrossRouterTimeline();
    std::vector<netlab::SimulationEvent> eventsB = buildRealCrossRouterTimeline();

    netlab::SimulationSession sessionA(eventsA);
    netlab::SimulationSession sessionB(eventsB);

    bool identical = eventsA.size() == eventsB.size();
    for (size_t i = 0; identical && i < eventsA.size(); ++i) {
        const netlab::SimulationEvent* a = sessionA.stepForward();
        const netlab::SimulationEvent* b = sessionB.stepForward();
        if (a == nullptr || b == nullptr || a->description != b->description || a->frameBytes != b->frameBytes) {
            identical = false;
        }
    }
    check(identical, "simulation-session: stepping through two independently-run but identical simulations yields identical events at every step (deterministic replay)");
}

int main() {
    testStepForwardRevealsEventsInOrder();
    testStepBackwardReversesInOrder();
    testJumpToAndReset();
    testReplayingTheIdenticalSimulationProducesIdenticalSteppedEvents();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
