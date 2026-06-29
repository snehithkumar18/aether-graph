#include "graph_coordinator.h"
#include "backup_utility.h"
#include "performance_metrics.h"
#include "query_compiler.h"
#include <cassert>
#include <iostream>

void run_coordinator_and_backup_tests() {
    AetherGraph::DiskManager disk_mgr("test_coord.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);
    AetherGraph::TransactionManager tm;
    AetherGraph::QueryOptimizer optimizer(AetherGraph::GraphStatistics{});
    AetherGraph::QueryCompiler compiler(ge, optimizer);

    // 1. Test QueryCompiler
    AetherGraph::ParsedQuery query;
    query.type = AetherGraph::QueryType::MATCH_NODE;
    query.node_label = "User";
    auto physical_op = compiler.compile(query);
    assert(physical_op != nullptr);

    // 2. Test BackupUtility
    std::string backup_dir = ".";
    AetherGraph::WalManager wal("test_coord_wal.log");
    AetherGraph::BackupUtility backup(ge, wal, backup_dir);

    ge.create_node("User");
    bool backup_ok = backup.create_full_backup(1);
    assert(backup_ok);

    ge.clear();
    assert(ge.get_all_nodes().empty());

    bool restore_ok = backup.restore_backup(1);
    assert(restore_ok);
    assert(ge.get_all_nodes().size() == 1);

    // 3. Test PerformanceMetrics
    AetherGraph::PerformanceMetrics metrics;
    metrics.record_value("query_latency", 12.5);
    metrics.record_value("query_latency", 17.5);

    auto snap = metrics.get_snapshot("query_latency");
    assert(snap.count == 2);
    assert(snap.average == 15.0);
    assert(snap.min_val == 12.5);
    assert(snap.max_val == 17.5);

    std::remove("test_coord.db");
    std::remove("test_coord_wal.log");
    std::remove("backup_1.bin");
}
