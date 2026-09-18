#include "raft.hpp"

namespace dist {

namespace {

std::vector<uint8_t> encodeVoteRequest(uint64_t term, NodeId candidateId) {
    Encoder enc;
    enc.writeU64(term);
    enc.writeU32(candidateId);
    return enc.data();
}

bool decodeVoteRequest(const std::vector<uint8_t>& bytes, uint64_t& term, NodeId& candidateId) {
    Decoder dec(bytes);
    return dec.readU64(term) && dec.readU32(candidateId) && dec.atEnd();
}

std::vector<uint8_t> encodeVoteResponse(uint64_t term, bool granted) {
    Encoder enc;
    enc.writeU64(term);
    enc.writeU8(granted ? 1 : 0);
    return enc.data();
}

bool decodeVoteResponse(const std::vector<uint8_t>& bytes, uint64_t& term, bool& granted) {
    Decoder dec(bytes);
    uint8_t g;
    if (!dec.readU64(term) || !dec.readU8(g) || !dec.atEnd()) return false;
    granted = g != 0;
    return true;
}

std::vector<uint8_t> encodeHeartbeat(uint64_t term, NodeId leaderId) {
    Encoder enc;
    enc.writeU64(term);
    enc.writeU32(leaderId);
    return enc.data();
}

bool decodeHeartbeat(const std::vector<uint8_t>& bytes, uint64_t& term, NodeId& leaderId) {
    Decoder dec(bytes);
    return dec.readU64(term) && dec.readU32(leaderId) && dec.atEnd();
}

std::vector<uint8_t> encodeHeartbeatAck(uint64_t term, bool accepted) {
    Encoder enc;
    enc.writeU64(term);
    enc.writeU8(accepted ? 1 : 0);
    return enc.data();
}

}  // namespace

RaftNode::RaftNode(
    NodeId self,
    std::vector<NodeId> peers,
    uint64_t timeoutSeed,
    uint32_t minElectionTimeoutTicks,
    uint32_t maxElectionTimeoutTicks,
    uint32_t heartbeatIntervalTicks
)
    : self(self),
      peers(std::move(peers)),
      timeoutRngState(timeoutSeed == 0 ? 0x2545F4914F6CDD1DULL : timeoutSeed),
      minElectionTimeoutTicks(minElectionTimeoutTicks),
      maxElectionTimeoutTicks(maxElectionTimeoutTicks),
      heartbeatIntervalTicks(heartbeatIntervalTicks),
      ticksUntilElectionTimeout(0),
      ticksUntilNextHeartbeat(heartbeatIntervalTicks) {
    resetElectionTimer();
}

uint32_t RaftNode::randomElectionTimeout() {
    // xorshift64*, independent of SimulatedNetwork's own RNG so a
    // cluster's election-timeout jitter and its network fault
    // decisions are two separately seeded, separately reproducible
    // sources of randomness — changing one without the other doesn't
    // perturb the other's sequence.
    timeoutRngState ^= timeoutRngState >> 12;
    timeoutRngState ^= timeoutRngState << 25;
    timeoutRngState ^= timeoutRngState >> 27;
    uint64_t r = timeoutRngState * 0x2545F4914F6CDD1DULL;

    uint32_t range = maxElectionTimeoutTicks - minElectionTimeoutTicks + 1;
    return minElectionTimeoutTicks + static_cast<uint32_t>(r % range);
}

void RaftNode::resetElectionTimer() {
    ticksUntilElectionTimeout = randomElectionTimeout();
}

void RaftNode::becomeFollower(uint64_t observedTerm) {
    currentTerm = observedTerm;
    currentRole = RaftRole::Follower;
    votedForThisTerm.reset();
    votesReceived.clear();
}

void RaftNode::wireHandlers() {
    rpcServer.registerMethod("RequestVote", [this](const std::vector<uint8_t>& payload) {
        return handleRequestVote(payload);
    });
    rpcServer.registerMethod("Heartbeat", [this](const std::vector<uint8_t>& payload) {
        return handleHeartbeat(payload);
    });
}

std::vector<uint8_t> RaftNode::handleRequestVote(const std::vector<uint8_t>& payload) {
    uint64_t requestTerm;
    NodeId candidateId;
    if (!decodeVoteRequest(payload, requestTerm, candidateId)) {
        return encodeVoteResponse(currentTerm, false);
    }

    if (requestTerm > currentTerm) {
        becomeFollower(requestTerm);
    }

    bool granted = false;
    if (requestTerm == currentTerm &&
        (!votedForThisTerm.has_value() || *votedForThisTerm == candidateId)) {
        votedForThisTerm = candidateId;
        granted = true;
        // Granting a vote is evidence of a live candidate contending
        // this term — reset our own timer so we don't immediately
        // start a competing election in the same term we just voted
        // in, one of the concrete details that makes elections
        // actually converge instead of splitting forever.
        resetElectionTimer();
    }

    return encodeVoteResponse(currentTerm, granted);
}

std::vector<uint8_t> RaftNode::handleHeartbeat(const std::vector<uint8_t>& payload) {
    uint64_t requestTerm;
    NodeId leaderId;
    if (!decodeHeartbeat(payload, requestTerm, leaderId)) {
        return encodeHeartbeatAck(currentTerm, false);
    }

    if (requestTerm < currentTerm) {
        // A heartbeat from a stale leader (an old term) is rejected —
        // it must not reset our timer or otherwise be treated as
        // legitimate.
        return encodeHeartbeatAck(currentTerm, false);
    }

    // A valid heartbeat for our term or later: accept this node as
    // leader for that term, revert to Follower if we were a Candidate
    // or (implausibly, but handled) a stale Leader, and reset our
    // election timer — the mechanism that suppresses further
    // elections while a leader is alive and reachable.
    if (requestTerm > currentTerm || currentRole != RaftRole::Follower) {
        becomeFollower(requestTerm);
    }
    resetElectionTimer();

    return encodeHeartbeatAck(currentTerm, true);
}

void RaftNode::startElection(SimulatedNetwork& network) {
    ++currentTerm;
    currentRole = RaftRole::Candidate;
    votedForThisTerm = self;
    votesReceived.clear();
    votesReceived.insert(self);
    resetElectionTimer();

    auto request = encodeVoteRequest(currentTerm, self);
    RpcRequest rpcRequest{
        static_cast<RequestId>(currentTerm) * 1000000ULL + self,  // unique-enough per (term, candidate)
        "RequestVote",
        request
    };
    auto bytes = serializeRequest(rpcRequest);

    for (NodeId peer : peers) {
        network.send(self, peer, bytes);
    }

    // A single-node cluster (no peers) wins its own vote immediately.
    if (peers.empty()) {
        becomeLeader(network);
    }
}

void RaftNode::becomeLeader(SimulatedNetwork& network) {
    currentRole = RaftRole::Leader;
    ticksUntilNextHeartbeat = 0;  // send the first heartbeat immediately
    sendHeartbeats(network);
    ticksUntilNextHeartbeat = heartbeatIntervalTicks;
}

void RaftNode::sendHeartbeats(SimulatedNetwork& network) {
    auto bytes = serializeRequest(RpcRequest{
        static_cast<RequestId>(currentTerm) * 1000000ULL + self,
        "Heartbeat",
        encodeHeartbeat(currentTerm, self)
    });

    for (NodeId peer : peers) {
        network.send(self, peer, bytes);
    }
}

void RaftNode::processIncomingResponses(SimulatedNetwork& network) {
    for (auto& [from, bytes] : network.receiveAll(responsePort(self))) {
        RpcResponse response;
        if (!parseResponse(bytes, response) || !response.ok) {
            continue;
        }

        uint64_t responderTerm;
        bool granted;
        if (!decodeVoteResponse(response.payload, responderTerm, granted)) {
            // Not a vote response this node cares about (e.g. a
            // heartbeat ack, which v1's leader doesn't need to act on
            // beyond having drained it from the inbox above).
            continue;
        }

        if (responderTerm > currentTerm) {
            becomeFollower(responderTerm);
            continue;
        }

        if (currentRole == RaftRole::Candidate &&
            responderTerm == currentTerm && granted) {
            votesReceived.insert(from);

            uint32_t clusterSize = static_cast<uint32_t>(peers.size()) + 1;
            uint32_t majority = clusterSize / 2 + 1;

            if (static_cast<uint32_t>(votesReceived.size()) >= majority) {
                becomeLeader(network);
            }
        }
    }
}

void RaftNode::onTick(SimulatedNetwork& network) {
    if (currentRole == RaftRole::Leader) {
        if (ticksUntilNextHeartbeat > 0) --ticksUntilNextHeartbeat;
        if (ticksUntilNextHeartbeat == 0) {
            sendHeartbeats(network);
            ticksUntilNextHeartbeat = heartbeatIntervalTicks;
        }
        return;
    }

    if (ticksUntilElectionTimeout > 0) --ticksUntilElectionTimeout;
    if (ticksUntilElectionTimeout == 0) {
        startElection(network);
    }
}

// --------------------------------------------------------------
// RaftCluster
// --------------------------------------------------------------

RaftCluster::RaftCluster(
    uint32_t nodeCount,
    uint64_t networkSeed,
    FaultConfig faults,
    uint64_t timeoutSeed
)
    : net(networkSeed, faults) {
    nodes.reserve(nodeCount);

    for (uint32_t i = 0; i < nodeCount; ++i) {
        std::vector<NodeId> peers;
        for (uint32_t j = 0; j < nodeCount; ++j) {
            if (j != i) peers.push_back(j);
        }

        // Each node's election-timeout RNG is seeded from the cluster
        // seed mixed with its own id, so every node has an
        // independent-looking but still fully deterministic timeout
        // sequence for a given (timeoutSeed) — without this, every
        // node would roll the identical sequence of "random" timeouts
        // and elections could deadlock in lockstep forever.
        uint64_t nodeSeed = timeoutSeed ^ (static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL);

        nodes.emplace_back(
            i, std::move(peers), nodeSeed,
            /*minElectionTimeoutTicks=*/10, /*maxElectionTimeoutTicks=*/20,
            /*heartbeatIntervalTicks=*/3
        );
        nodes.back().wireHandlers();
    }
}

void RaftCluster::tick() {
    net.tick();

    for (auto& n : nodes) {
        pumpServer(net, n.id(), n.server());
    }
    for (auto& n : nodes) {
        n.processIncomingResponses(net);
    }
    for (auto& n : nodes) {
        n.onTick(net);
    }
}

void RaftCluster::tickMany(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) tick();
}

RaftNode& RaftCluster::node(NodeId id) {
    return nodes[id];
}

std::vector<NodeId> RaftCluster::currentLeaders() const {
    std::vector<NodeId> leaders;
    for (const auto& n : nodes) {
        if (n.role() == RaftRole::Leader) leaders.push_back(n.id());
    }
    return leaders;
}

bool RaftCluster::hasLeader() const {
    return !currentLeaders().empty();
}

}  // namespace dist
