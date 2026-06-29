#ifndef AETHER_GRAPH_GRAPH_ALGORITHMS_H
#define AETHER_GRAPH_GRAPH_ALGORITHMS_H

#include "graph_engine.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace AetherGraph {

struct DijkstraResult {
    std::vector<node_id_t> path;
    double total_weight;
};

class GraphAlgorithms {
public:
    static std::unordered_map<node_id_t, double> page_rank(GraphEngine& ge, double damping_factor = 0.85, int max_iterations = 100, double convergence_threshold = 1e-6);
    static DijkstraResult dijkstra(GraphEngine& ge, node_id_t start_node, node_id_t end_node, const std::string& weight_property);
    static std::unordered_map<node_id_t, node_id_t> connected_components(GraphEngine& ge);
    static std::unordered_map<node_id_t, double> betweenness_centrality(GraphEngine& ge);
    static std::unordered_map<node_id_t, node_id_t> louvain_communities(GraphEngine& ge);
    static std::vector<node_id_t> topological_sort(GraphEngine& ge);
    static std::unordered_map<node_id_t, uint32_t> count_triangles(GraphEngine& ge, uint32_t& total_triangles);
    static std::unordered_map<node_id_t, uint32_t> k_core_decomposition(GraphEngine& ge);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_ALGORITHMS_H
