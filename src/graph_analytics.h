#ifndef AETHER_GRAPH_GRAPH_ANALYTICS_H
#define AETHER_GRAPH_GRAPH_ANALYTICS_H

#include "graph_engine.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace AetherGraph {

struct DegreeStats {
    uint32_t min_degree;
    uint32_t max_degree;
    double mean_degree;
    double median_degree;
    double std_dev_degree;
    std::vector<uint32_t> degree_histogram;
};

struct GraphDensityInfo {
    double density;
    uint32_t node_count;
    uint32_t edge_count;
    double avg_degree;
    bool is_sparse;
};

struct LinkPredictionScore {
    node_id_t src;
    node_id_t dest;
    double score;
};

class GraphAnalytics {
public:
    static DegreeStats compute_degree_distribution(GraphEngine& ge);
    static double local_clustering_coefficient(GraphEngine& ge, node_id_t node_id);
    static double global_clustering_coefficient(GraphEngine& ge);
    static GraphDensityInfo compute_density(GraphEngine& ge);
    static bool compute_diameter_and_radius(GraphEngine& ge, uint32_t& diameter, uint32_t& radius);
    static std::vector<std::vector<node_id_t>> strongly_connected_components(GraphEngine& ge);
    static bool are_isomorphic(GraphEngine& ge1, GraphEngine& ge2);
    static std::vector<node_id_t> random_walk(GraphEngine& ge, node_id_t start_node, size_t length, double restart_prob = 0.15);
    static std::vector<node_id_t> influence_maximization_greedy(GraphEngine& ge, size_t seed_size, size_t mc_simulations = 100);
    static std::vector<LinkPredictionScore> predict_links(GraphEngine& ge, const std::string& method = "jaccard");
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_ANALYTICS_H
