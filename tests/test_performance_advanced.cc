#include "graph_engine.h"
#include "graph_algorithms.h"
#include "property_index.h"
#include "vector_index.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <cassert>

void run_advanced_performance_benchmarks() {
    std::cout << "[BENCHMARK] Starting Advanced Performance Benchmarks...\n";

    AetherGraph::DiskManager disk_mgr("test_perf_adv.db");
    AetherGraph::BufferPoolManager bpm(100, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    // 1. Benchmark B+ Tree Property Index Lookups
    AetherGraph::PropertyIndex index("age", 8);
    std::vector<Node*> nodes;
    
    // Insert 1000 nodes with random ages
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> age_dist(18, 70);
    
    for (int i = 0; i < 1000; ++i) {
        Node* n = ge.create_node("User");
        int age = age_dist(rng);
        n->properties["age"] = AetherGraph::Variant(age);
        nodes.push_back(n);
        index.insert(std::to_string(age), n->id);
    }

    auto start_idx = std::chrono::high_resolution_clock::now();
    size_t found_nodes = 0;
    for (int i = 0; i < 500; ++i) {
        int search_age = age_dist(rng);
        auto results = index.search(std::to_string(search_age));
        found_nodes += results.size();
    }
    auto end_idx = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff_idx = end_idx - start_idx;
    std::cout << "[BENCHMARK] 500 B+ Tree Index lookups completed in " 
              << diff_idx.count() << " ms. Found " << found_nodes << " nodes.\n";

    // 2. Benchmark Range Queries on Index
    auto start_range = std::chrono::high_resolution_clock::now();
    size_t range_nodes = 0;
    for (int i = 0; i < 100; ++i) {
        auto results = index.range_search("20", "30");
        range_nodes += results.size();
    }
    auto end_range = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff_range = end_range - start_range;
    std::cout << "[BENCHMARK] 100 B+ Tree Range queries completed in " 
              << diff_range.count() << " ms. Retrieved " << range_nodes << " nodes.\n";

    // 3. Benchmark High-Dimensional Vector KNN Search
    AetherGraph::VectorIndex vec_index("embedding", 64);
    std::normal_distribution<float> norm_dist(0.0f, 1.0f);
    
    for (int i = 0; i < 500; ++i) {
        std::vector<float> vec(64);
        for (int d = 0; d < 64; ++d) {
            vec[d] = norm_dist(rng);
        }
        vec_index.add_vector(static_cast<node_id_t>(i), vec);
    }
    vec_index.build_index(4);

    std::vector<float> query_vec(64);
    for (int d = 0; d < 64; ++d) {
        query_vec[d] = norm_dist(rng);
    }

    auto start_vec = std::chrono::high_resolution_clock::now();
    auto knn = vec_index.search_knn(query_vec, 10, "l2");
    auto end_vec = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff_vec = end_vec - start_vec;
    std::cout << "[BENCHMARK] IVF-Flat Vector KNN search completed in " 
              << diff_vec.count() << " ms. Closest neighbor: ID=" << knn[0].node_id 
              << " distance=" << knn[0].distance << "\n";

    std::remove("test_perf_adv.db");
}
