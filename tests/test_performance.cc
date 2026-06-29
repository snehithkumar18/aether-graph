#include "graph_engine.h"
#include "graph_algorithms.h"
#include "graph_serializer.h"
#include "vector_index.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <cassert>

static void run_benchmark_insertion(GraphEngine& ge, size_t node_count, size_t edge_count) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<Node*> created_nodes;
    created_nodes.reserve(node_count);
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> label_dist(0, 3);
    std::vector<std::string> labels = {"User", "Group", "Post", "Comment"};

    for (size_t i = 0; i < node_count; ++i) {
        std::string l = labels[label_dist(rng)];
        Node* n = ge.create_node(l);
        n->properties["index"] = Variant(static_cast<int>(i));
        n->properties["weight"] = Variant(static_cast<float>(i * 0.5f));
        created_nodes.push_back(n);
    }

    std::uniform_int_distribution<size_t> node_idx_dist(0, node_count - 1);
    std::uniform_int_distribution<int> edge_type_dist(0, 2);
    std::vector<std::string> edge_types = {"KNOWS", "FOLLOWS", "REPLIES"};

    for (size_t i = 0; i < edge_count; ++i) {
        size_t src = node_idx_dist(rng);
        size_t dest = node_idx_dist(rng);
        if (src != dest) {
            std::string t = edge_types[edge_type_dist(rng)];
            ge.create_edge(created_nodes[src]->id, created_nodes[dest]->id, t);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "[BENCHMARK] Inserted " << node_count << " nodes and " 
              << edge_count << " edges in " << duration.count() << " ms.\n";
}

static void run_benchmark_pagerank(GraphEngine& ge) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto ranks = GraphAlgorithms::page_rank(ge, 0.85, 20, 1e-4);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "[BENCHMARK] PageRank on " << ge.get_all_nodes().size() 
              << " nodes completed in " << duration.count() << " ms. Result size: " 
              << ranks.size() << "\n";
}

static void run_benchmark_dijkstra(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    if (nodes.size() < 2) return;

    std::vector<node_id_t> nids;
    for (const auto& [nid, n] : nodes) {
        nids.push_back(nid);
    }

    std::mt19937 rng(1337);
    std::uniform_int_distribution<size_t> dist(0, nids.size() - 1);

    auto start = std::chrono::high_resolution_clock::now();

    size_t runs = 50;
    size_t found_paths = 0;
    for (size_t i = 0; i < runs; ++i) {
        node_id_t src = nids[dist(rng)];
        node_id_t dest = nids[dist(rng)];
        if (src != dest) {
            auto res = GraphAlgorithms::dijkstra(ge, src, dest, "weight");
            if (res.total_weight >= 0) {
                found_paths++;
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "[BENCHMARK] Dijkstra Shortest Path (" << runs 
              << " runs) completed in " << duration.count() << " ms. Paths found: " 
              << found_paths << "\n";
}

static void run_benchmark_vector_search(size_t num_vectors) {
    auto start = std::chrono::high_resolution_clock::now();
    
    VectorIndex v_idx("embedding", 128);
    std::mt19937 rng(999);
    std::normal_distribution<float> norm_dist(0.0f, 1.0f);

    for (size_t i = 0; i < num_vectors; ++i) {
        std::vector<float> vec(128);
        float norm = 0.0f;
        for (size_t d = 0; d < 128; ++d) {
            vec[d] = norm_dist(rng);
            norm += vec[d] * vec[d];
        }
        norm = std::sqrt(norm);
        if (norm > 0.0f) {
            for (size_t d = 0; d < 128; ++d) vec[d] /= norm;
        }
        v_idx.add_vector(static_cast<node_id_t>(i + 1), vec);
    }

    v_idx.build_index(8);

    std::vector<float> query(128, 0.0f);
    query[0] = 1.0f; // test query

    auto knn = v_idx.search_knn(query, 5, "l2");

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "[BENCHMARK] Vector Index search over " << num_vectors 
              << " vectors completed in " << duration.count() << " ms. KNN size: " 
              << knn.size() << "\n";
}

void run_performance_benchmarks() {
    std::cout << "--------------------------------------------------\n";
    std::cout << "          STARTING PERFORMANCE BENCHMARKS         \n";
    std::cout << "--------------------------------------------------\n";

    DiskManager disk_mgr("benchmark.db");
    BufferPoolManager bpm(50, disk_mgr);
    GraphEngine ge(bpm);

    // Run tests with progressive scale
    run_benchmark_insertion(ge, 500, 1500);
    run_benchmark_pagerank(ge);
    run_benchmark_dijkstra(ge);

    run_benchmark_insertion(ge, 1000, 4000);
    run_benchmark_pagerank(ge);
    run_benchmark_dijkstra(ge);

    run_benchmark_vector_search(1000);
    run_benchmark_vector_search(3000);

    std::remove("benchmark.db");
    std::cout << "--------------------------------------------------\n";
}
