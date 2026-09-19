#pragma once

#include "ip.hpp"

// The first NetLab gamified mission: a real, data-driven objective/
// validation/failure/completion evaluator built directly on
// ip.hpp's sendIcmpEcho — not a static pass/fail stub. See
// docs/ADR/0028-netlab-ip-arp-routing-mission.md.

namespace netlab {

struct MissionResult {
    bool success = false;
    std::string reason;  // always set: the specific completion or failure reason
    PingResult pingResult;  // the underlying simulation, for inspection/timeline replay
};

// Mission: "Connect Two Networks."
// Objective: configure hostA and hostB on different subnets, with a
// router between them, so hostA can successfully ping hostB.
MissionResult evaluateConnectTwoNetworksMission(
    Topology& topology, const std::string& hostAId, const std::string& hostBId
);

}  // namespace netlab
