#include "distributed_txn.h"
#include <cassert>
#include <iostream>

void run_distributed_tests() {
    AetherGraph::DiskManager disk_mgr("test_dist.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);
    AetherGraph::TransactionManager tm;

    // 1. Setup Coordinator and Participants
    AetherGraph::DistributedTransactionCoordinator coordinator(5000);
    coordinator.register_participant(1);
    coordinator.register_participant(2);

    AetherGraph::DistributedTransactionParticipant part1(1, ge, tm);
    AetherGraph::DistributedTransactionParticipant part2(2, ge, tm);

    // 2. Execute 2PC Flow
    bool prep1 = part1.prepare(5000);
    bool prep2 = part2.prepare(5000);
    assert(prep1 && prep2);

    bool commit_ok = coordinator.execute_two_phase_commit();
    assert(commit_ok);

    part1.commit(5000);
    part2.commit(5000);

    std::remove("test_dist.db");
}
