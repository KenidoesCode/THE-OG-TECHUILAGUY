#pragma once

#include "../rpc/rpc.hpp"

#include <optional>
#include <unordered_set>
#include <vector>

// Raft leader election (Ongaro & Ousterhout, "In Search of an
// Understandable Consensus Algorithm") built directly on the RPC +
// deterministic simulation foundation from ADR 0008. This is
// leader-election only — no log replication, no committed entries, no
// state machine application yet. See docs/ADR/0009-raft-leader-election.md
// for the exact scope and the safety/liveness properties actually
// tested.

namespace dist {

enum class RaftRole {
    Follower,
    Candidate,
    Leader,
};

// One Raft server's state and message handling. A RaftCluster (below)
// owns one of these per node, wires them all to a shared
// SimulatedNetwork, and drives them forward tick by tick.
class RaftNode {
public:
    RaftNode(
        NodeId self,
        std::vector<NodeId> peers,  // every OTHER node in the cluster
        uint64_t timeoutSeed,
        uint32_t minElectionTimeoutTicks,
        uint32_t maxElectionTimeoutTicks,
        uint32_t heartbeatIntervalTicks
    );

    // Registers this node's RequestVote/Heartbeat handlers on its own
    // RpcServer — call once before the cluster starts ticking.
    void wireHandlers();

    RpcServer& server() { return rpcServer; }

    // Advances this node's own election/heartbeat timers by one tick
    // and, if the election timeout has just elapsed (and this node
    // isn't already a leader), starts a new election by broadcasting
    // RequestVote to every peer. Must be called once per simulation
    // tick, after the network has delivered that tick's messages and
    // after wireHandlers()-installed handlers have already run via
    // pumpServer() for any requests that arrived this tick.
    void onTick(SimulatedNetwork& network);

    // Processes every response waiting in this node's own inbox
    // (RequestVote responses to a candidacy this node started) —
    // separate from onTick() because responses arrive asynchronously
    // relative to when this node's own timer fires, exactly like a
    // real Raft implementation's event loop would treat them as two
    // independent input sources.
    void processIncomingResponses(SimulatedNetwork& network);

    RaftRole role() const { return currentRole; }
    uint64_t term() const { return currentTerm; }
    std::optional<NodeId> votedFor() const { return votedForThisTerm; }
    NodeId id() const { return self; }

private:
    NodeId self;
    std::vector<NodeId> peers;
    RpcServer rpcServer;

    RaftRole currentRole = RaftRole::Follower;
    uint64_t currentTerm = 0;
    std::optional<NodeId> votedForThisTerm;

    uint64_t timeoutRngState;
    uint32_t minElectionTimeoutTicks;
    uint32_t maxElectionTimeoutTicks;
    uint32_t heartbeatIntervalTicks;

    uint32_t ticksUntilElectionTimeout;
    uint32_t ticksUntilNextHeartbeat;

    // Votes received in the candidacy currently in progress (only
    // meaningful while currentRole == Candidate); reset every time a
    // new election starts.
    std::unordered_set<NodeId> votesReceived;

    uint32_t randomElectionTimeout();
    void resetElectionTimer();
    void becomeFollower(uint64_t observedTerm);
    void startElection(SimulatedNetwork& network);
    void becomeLeader(SimulatedNetwork& network);
    void sendHeartbeats(SimulatedNetwork& network);

    // RPC handler bodies (registered with rpcServer in wireHandlers()).
    std::vector<uint8_t> handleRequestVote(const std::vector<uint8_t>& payload);
    std::vector<uint8_t> handleHeartbeat(const std::vector<uint8_t>& payload);
};

// Owns a full cluster of RaftNodes sharing one SimulatedNetwork and
// drives every node forward together, one simulated tick at a time —
// the deterministic-simulation harness a test actually interacts
// with.
class RaftCluster {
public:
    RaftCluster(
        uint32_t nodeCount,
        uint64_t networkSeed,
        FaultConfig faults = {},
        uint64_t timeoutSeed = 0xC0FFEE
    );

    void tick();
    void tickMany(uint32_t count);

    SimulatedNetwork& network() { return net; }
    RaftNode& node(NodeId id);

    // Returns the set of nodes currently believing they are Leader —
    // in a correct Raft implementation, at most one node per term ever
    // has this be true, but this function does not itself enforce
    // that; tests check it explicitly (see
    // testAtMostOneLeaderPerTermAcrossWholeRun for how a genuine
    // safety violation would be caught).
    std::vector<NodeId> currentLeaders() const;

    // True once some node holds RaftRole::Leader.
    bool hasLeader() const;

private:
    SimulatedNetwork net;
    std::vector<RaftNode> nodes;
};

}  // namespace dist
