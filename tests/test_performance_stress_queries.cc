#include "graph_server.h"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

void run_performance_stress_queries_tests() {
    std::cout << "[BENCHMARK] Starting Stress Query Benchmarks...\n";

    AetherGraph::DiskManager disk_mgr("test_stress_q.db");
    AetherGraph::BufferPoolManager bpm(50, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);
    AetherGraph::TransactionManager tm;
    AetherGraph::ConcurrencyControl cc;
    AetherGraph::GraphServer server(ge, tm, cc);

    // Populate graph with initial data
    uint32_t setup_client = server.connect_client();
    server.execute_query(setup_client, "CREATE (n:User)");
    server.disconnect_client(setup_client);

    std::atomic<bool> stop_flag(false);
    std::atomic<size_t> query_count(0);

    auto worker = [&]() {
        uint32_t client_id = server.connect_client();
        while (!stop_flag.load()) {
            std::string res = server.execute_query(client_id, "MATCH (n:User) WHERE n.name = 'Bob' RETURN n");
            assert(res.find("success") != std::string::npos);
            query_count++;
            std::this_thread::yield();
        }
        server.disconnect_client(client_id);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.push_back(std::thread(worker));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop_flag.store(true);

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cout << "[BENCHMARK] Stress queries completed. Executed " 
              << query_count.load() << " Cypher queries in 500ms.\n";

    std::remove("test_stress_q.db");
}
