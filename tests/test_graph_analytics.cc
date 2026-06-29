#include "graph_analytics.h"
#include <cassert>
#include <iostream>

void run_graph_analytics_tests() {
    AetherGraph::DiskManager disk_mgr("test_an.db");
    AetherGraph::BufferPoolManager bpm(10, disk_mgr);
    AetherGraph::GraphEngine ge1(bpm);

    // 1. Test Isomorphism on identical graphs
    auto* n1 = ge1.create_node("User");
    auto* n2 = ge1.create_node("User");
    ge1.create_edge(n1->id, n2->id, "KNOWS");

    AetherGraph::GraphEngine ge2(bpm);
    auto* m1 = ge2.create_node("User");
    auto* m2 = ge2.create_node("User");
    ge2.create_edge(m1->id, m2->id, "KNOWS");

    bool iso = AetherGraph::GraphAnalytics::are_isomorphic(ge1, ge2);
    assert(iso);

    // Make them non-isomorphic by adding a node to ge2
    ge2.create_node("User");
    assert(!AetherGraph::GraphAnalytics::are_isomorphic(ge1, ge2));

    // 2. Test Tarjan SCC
    ge1.clear();
    auto* ta = ge1.create_node("A");
    auto* tb = ge1.create_node("B");
    auto* tc = ge1.create_node("C");
    ge1.create_edge(ta->id, tb->id, "LINK");
    ge1.create_edge(tb->id, tc->id, "LINK");
    ge1.create_edge(tc->id, ta->id, "LINK"); // 3-node cycle SCC

    auto sccs = AetherGraph::GraphAnalytics::strongly_connected_components(ge1);
    assert(sccs.size() == 1);
    assert(sccs[0].size() == 3);

    // 3. Test Degree stats
    auto stats = AetherGraph::GraphAnalytics::compute_degree_distribution(ge1);
    assert(stats.min_degree == 2);
    assert(stats.max_degree == 2);
    assert(stats.mean_degree == 2.0);

    // 4. Test Density
    auto density = AetherGraph::GraphAnalytics::compute_density(ge1);
    assert(std::abs(density.density - 0.5) < 1e-5); // 3 edges out of 6 possible directed edges

    // 5. Test Random Walk
    auto path = AetherGraph::GraphAnalytics::random_walk(ge1, ta->id, 10, 0.0);
    assert(path.size() == 10);
    assert(path[0] == ta->id);

    // 6. Test Link Prediction
    ge1.clear();
    auto* p1 = ge1.create_node("User");
    auto* p2 = ge1.create_node("User");
    auto* p3 = ge1.create_node("User");
    ge1.create_edge(p1->id, p2->id, "KNOWS");
    ge1.create_edge(p2->id, p3->id, "KNOWS");

    auto predictions = AetherGraph::GraphAnalytics::predict_links(ge1, "jaccard");
    assert(!predictions.empty());
    assert(predictions[0].src == p1->id || predictions[0].src == p3->id);
    assert(predictions[0].dest == p1->id || predictions[0].dest == p3->id);

    std::remove("test_an.db");
}
