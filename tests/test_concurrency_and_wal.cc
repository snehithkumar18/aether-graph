#include "wal_manager.h"
#include "concurrency_control.h"
#include <cassert>
#include <iostream>

void run_concurrency_and_wal_tests() {
    // 1. Test Concurrency Control Lock Table & Deadlocks
    AetherGraph::ConcurrencyControl cc;
    
    // Acquire Shared Lock
    bool ok1 = cc.acquire_lock(100, 1, AetherGraph::LockMode::SHARED);
    assert(ok1);
    
    // Concurrently acquire Shared Lock on same resource
    bool ok2 = cc.acquire_lock(200, 1, AetherGraph::LockMode::SHARED);
    assert(ok2);

    // Acquire Exclusive Lock should fail due to conflict
    bool ok3 = cc.acquire_lock(300, 1, AetherGraph::LockMode::EXCLUSIVE);
    assert(!ok3);

    // Deadlock detection cycle: Txn 100 waits for 200, 200 waits for 100
    // We simulate this wait-for graph structure
    bool lock_a = cc.acquire_lock(100, 2, AetherGraph::LockMode::EXCLUSIVE);
    bool lock_b = cc.acquire_lock(200, 3, AetherGraph::LockMode::EXCLUSIVE);
    assert(lock_a && lock_b);

    // Conflict requests to build cycle
    cc.acquire_lock(100, 3, AetherGraph::LockMode::EXCLUSIVE); // 100 waits for 200
    cc.acquire_lock(200, 2, AetherGraph::LockMode::EXCLUSIVE); // 200 waits for 100

    std::vector<txn_id_t> cycle;
    bool deadlock = cc.detect_deadlock(cycle);
    assert(deadlock);
    assert(cycle.size() >= 2);

    cc.release_all_locks(100);
    cc.release_all_locks(200);

    // 2. Test WAL Durability Log & Recovery
    std::string log_file = "test_wal.log";
    std::remove(log_file.c_str());

    AetherGraph::DiskManager disk_mgr("test_wal.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    
    {
        AetherGraph::GraphEngine ge(bpm);
        AetherGraph::WalManager wal(log_file);

        // Perform some ops and write to WAL
        Node* n1 = ge.create_node("User");
        wal.append_record(1, AetherGraph::WalOpType::INSERT_NODE, n1->id, 0, "User", "", AetherGraph::Variant());

        Node* n2 = ge.create_node("User");
        wal.append_record(1, AetherGraph::WalOpType::INSERT_NODE, n2->id, 0, "User", "", AetherGraph::Variant());

        Edge* edge = ge.create_edge(n1->id, n2->id, "KNOWS");
        wal.append_record(1, AetherGraph::WalOpType::INSERT_EDGE, n1->id, n2->id, "KNOWS", "", AetherGraph::Variant());

        wal.force_flush();
    }

    // Restore from WAL on a fresh engine
    {
        AetherGraph::GraphEngine ge_recovered(bpm);
        AetherGraph::WalManager wal(log_file);
        
        bool rec_ok = wal.recover(ge_recovered);
        assert(rec_ok);
        assert(ge_recovered.get_all_nodes().size() == 2);
        assert(ge_recovered.get_all_edges().size() == 1);
    }

    std::remove(log_file.c_str());
    std::remove("test_wal.db");
}
