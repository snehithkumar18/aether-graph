#include "graph_algorithms.h"
#include <cassert>
#include <iostream>
#include <cmath>

void run_graph_algorithms_tests() {
    AetherGraph::DiskManager disk_mgr("test_algo.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge(bpm);

    // 1. Setup Nodes
    auto* n1 = ge.create_node("User");
    auto* n2 = ge.create_node("User");
    auto* n3 = ge.create_node("User");
    auto* n4 = ge.create_node("User");

    assert(n1 != nullptr);
    assert(n2 != nullptr);
    assert(n3 != nullptr);
    assert(n4 != nullptr);

    // 2. Setup Edges
    auto* e1 = ge.create_edge(n1->id, n2->id, "KNOWS");
    auto* e2 = ge.create_edge(n2->id, n3->id, "KNOWS");
    auto* e3 = ge.create_edge(n3->id, n1->id, "KNOWS"); // Cycle
    auto* e4 = ge.create_edge(n3->id, n4->id, "KNOWS"); // Directed path to n4

    // Set edge weight properties for Dijkstra
    e1->properties["weight"] = AetherGraph::Variant(1.5f);
    e2->properties["weight"] = AetherGraph::Variant(2.5f);
    e3->properties["weight"] = AetherGraph::Variant(0.5f);
    e4->properties["weight"] = AetherGraph::Variant(1.0f);

    // Test Dijkstra shortest path
    auto route = AetherGraph::GraphAlgorithms::dijkstra(ge, n1->id, n4->id, "weight");
    assert(route.path.size() == 4);
    assert(route.path[0] == n1->id);
    assert(route.path[1] == n2->id);
    assert(route.path[2] == n3->id);
    assert(route.path[3] == n4->id);
    assert(std::abs(route.total_weight - 5.0) < 1e-5);

    // Test Connected Components
    auto components = AetherGraph::GraphAlgorithms::connected_components(ge);
    assert(components.size() == 4);
    node_id_t root = components[n1->id];
    assert(components[n2->id] == root);
    assert(components[n3->id] == root);
    assert(components[n4->id] == root);

    // Test PageRank
    auto ranks = AetherGraph::GraphAlgorithms::page_rank(ge, 0.85, 50, 1e-6);
    assert(ranks.size() == 4);
    double sum = 0.0;
    for (const auto& [nid, rank] : ranks) {
        assert(rank > 0.0);
        sum += rank;
    }
    assert(std::abs(sum - 1.0) < 1e-4);

    // Test Triangle Counting
    uint32_t total_triangles = 0;
    auto triangles = AetherGraph::GraphAlgorithms::count_triangles(ge, total_triangles);
    assert(total_triangles == 1); // n1-n2-n3 forms one triangle
    assert(triangles[n1->id] == 1);
    assert(triangles[n2->id] == 1);
    assert(triangles[n3->id] == 1);
    assert(triangles[n4->id] == 0);

    // Test Topological Sort (should return empty due to cycles)
    auto ts_order = AetherGraph::GraphAlgorithms::topological_sort(ge);
    assert(ts_order.empty()); // Cycle exists

    // Clear cycle and test topological sort again
    ge.clear();
    auto* ta = ge.create_node("A");
    auto* tb = ge.create_node("B");
    auto* tc = ge.create_node("C");
    ge.create_edge(ta->id, tb->id, "DEPENDS");
    ge.create_edge(tb->id, tc->id, "DEPENDS");

    auto ts_valid = AetherGraph::GraphAlgorithms::topological_sort(ge);
    assert(ts_valid.size() == 3);
    assert(ts_valid[0] == ta->id);
    assert(ts_valid[1] == tb->id);
    assert(ts_valid[2] == tc->id);

    // Test k-core
    auto cores = AetherGraph::GraphAlgorithms::k_core_decomposition(ge);
    assert(cores.size() == 3);

    std::remove("test_algo.db");
}
