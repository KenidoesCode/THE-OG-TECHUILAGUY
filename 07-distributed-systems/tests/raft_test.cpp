// Real assertion-based tests for Raft leader election
// (raft/raft.cpp), built directly on the RPC + deterministic
// simulation foundation from ADR 0008. Hosted, no hardware dependency.

#include "../raft/raft.hpp"

#include <iostream>
#include <map>
#include <set>
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

}  // namespace

void testThreeNodeClusterEventuallyElectsExactlyOneLeader() {
    dist::RaftCluster cluster(3, /*networkSeed=*/1, {}, /*timeoutSeed=*/42);

    cluster.tickMany(200);

    auto leaders = cluster.currentLeaders();
    check(leaders.size() == 1,
          "raft: a 3-node cluster with no fault injection converges on "
          "exactly one leader within 200 ticks");
}

void testFiveNodeClusterEventuallyElectsExactlyOneLeader() {
    dist::RaftCluster cluster(5, /*networkSeed=*/7, {}, /*timeoutSeed=*/1234);

    cluster.tickMany(200);

    auto leaders = cluster.currentLeaders();
    check(leaders.size() == 1,
          "raft: a 5-node cluster with no fault injection converges on "
          "exactly one leader within 200 ticks");
}

void testSingleNodeClusterImmediatelyBecomesLeader() {
    dist::RaftCluster cluster(1, 1, {}, 1);

    // A single-node cluster becomes leader the moment its own
    // (randomized, up to maxElectionTimeoutTicks=20) election timeout
    // first fires — there are no peers to wait on, so that first
    // timeout is the only thing gating it. 30 ticks comfortably
    // exceeds the maximum possible wait for any seed.
    cluster.tickMany(30);

    check(cluster.node(0).role() == dist::RaftRole::Leader,
          "raft: a single-node cluster (no peers to wait on) becomes "
          "its own leader as soon as its own election timeout first fires");
}

void testAtMostOneLeaderPerTermAcrossWholeRun() {
    // The core Raft election-safety property: within any given term,
    // at most one node ever believes it is leader — checked at EVERY
    // tick throughout the run, not just at the end, since a genuine
    // safety bug (e.g. a vote-counting error) could produce a
    // momentary dual-leader situation that later resolves itself and
    // would be invisible to an end-of-run-only check.
    dist::RaftCluster cluster(5, 99, {}, 555);

    std::map<uint64_t, std::set<dist::NodeId>> leadersByTerm;
    bool violationFound = false;

    for (int i = 0; i < 300; ++i) {
        cluster.tick();

        for (dist::NodeId id = 0; id < 5; ++id) {
            dist::RaftNode& n = cluster.node(id);
            if (n.role() == dist::RaftRole::Leader) {
                leadersByTerm[n.term()].insert(id);
                if (leadersByTerm[n.term()].size() > 1) {
                    violationFound = true;
                }
            }
        }
    }

    check(!violationFound,
          "raft safety: no two nodes ever simultaneously believe they "
          "are leader for the same term, checked at every tick of a "
          "300-tick run, not merely at the end");
}

void testLeaderFailureTriggersNewElection() {
    dist::RaftCluster cluster(5, 3, {}, 777);
    cluster.tickMany(200);

    auto leadersBefore = cluster.currentLeaders();
    check(leadersBefore.size() == 1,
          "raft failover: a leader is established before simulating its failure");

    dist::NodeId failedLeader = leadersBefore[0];
    uint64_t termBefore = cluster.node(failedLeader).term();

    // Simulate the leader's total failure/isolation by partitioning it
    // from every other node — it can neither send heartbeats nor
    // receive anything, the concrete network-level meaning of "this
    // node is gone" in this simulation.
    for (dist::NodeId other = 0; other < 5; ++other) {
        if (other != failedLeader) {
            cluster.network().partition(failedLeader, other);
        }
    }

    cluster.tickMany(200);

    dist::NodeId newLeader = 0;
    bool foundNewLeader = false;
    for (dist::NodeId id = 0; id < 5; ++id) {
        if (id != failedLeader &&
            cluster.node(id).role() == dist::RaftRole::Leader) {
            newLeader = id;
            foundNewLeader = true;
        }
    }

    check(foundNewLeader,
          "raft failover: after the leader is isolated, the remaining "
          "4 nodes elect a new leader among themselves");
    check(foundNewLeader && cluster.node(newLeader).term() > termBefore,
          "raft failover: the new leader's term is strictly greater "
          "than the failed leader's term (elections always advance the term)");
}

void testDeterministicSameSeedProducesSameLeaderAndTerm() {
    auto runScenario = [](uint64_t networkSeed, uint64_t timeoutSeed) {
        dist::RaftCluster cluster(5, networkSeed, {}, timeoutSeed);
        cluster.tickMany(200);
        auto leaders = cluster.currentLeaders();
        dist::NodeId leader = leaders.empty() ? 999 : leaders[0];
        uint64_t term = leaders.empty() ? 0 : cluster.node(leader).term();
        return std::make_pair(leader, term);
    };

    auto run1 = runScenario(50, 60);
    auto run2 = runScenario(50, 60);
    auto run3 = runScenario(50, 61);  // different timeout seed only

    check(run1 == run2,
          "raft determinism: two clusters built from identical network "
          "and timeout seeds elect the identical leader in the identical term");

    // Not asserted to always differ (a different seed *could*
    // coincidentally produce the same outcome), but checked here as a
    // sanity signal that the timeout seed genuinely participates in
    // the outcome for this specific configuration.
    if (run1 != run3) {
        std::cout << "[PASS] raft determinism: a different timeout seed "
                     "(with the same network seed) produced a different "
                     "outcome in this configuration, confirming the "
                     "timeout seed genuinely drives node behavior\n";
    } else {
        std::cout << "[PASS] raft determinism: (informational) a "
                     "different timeout seed coincidentally converged "
                     "on the same leader/term here — not a failure, "
                     "since this isn't guaranteed to differ\n";
    }
}

void testElectionEventuallySucceedsUnderModerateMessageLoss() {
    // Raft's liveness argument depends on randomized election timeouts
    // eventually letting exactly one candidate's RequestVote round
    // reach a majority before another candidate's — moderate message
    // loss should slow convergence, not prevent it, given enough ticks.
    dist::FaultConfig faults;
    faults.dropProbability = 0.2;

    dist::RaftCluster cluster(5, 21, faults, 4242);

    cluster.tickMany(2000);

    check(cluster.hasLeader(),
          "raft liveness: a cluster still converges on a leader within "
          "2000 ticks despite a 20% message drop rate");
}

int main() {
    testThreeNodeClusterEventuallyElectsExactlyOneLeader();
    testFiveNodeClusterEventuallyElectsExactlyOneLeader();
    testSingleNodeClusterImmediatelyBecomesLeader();
    testAtMostOneLeaderPerTermAcrossWholeRun();
    testLeaderFailureTriggersNewElection();
    testDeterministicSameSeedProducesSameLeaderAndTerm();
    testElectionEventuallySucceedsUnderModerateMessageLoss();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
