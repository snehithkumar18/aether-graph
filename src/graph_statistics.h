#ifndef AETHER_GRAPH_GRAPH_STATISTICS_H
#define AETHER_GRAPH_GRAPH_STATISTICS_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace AetherGraph {

struct Bucket {
    float lower_bound;
    float upper_bound;
    uint32_t count;
};

class EquiDepthHistogram {
private:
    std::vector<Bucket> buckets_;
    uint32_t total_count_ = 0;

public:
    EquiDepthHistogram() = default;
    ~EquiDepthHistogram() = default;

    void build(const std::vector<float>& values, size_t num_buckets);
    double estimate_selectivity(float val, const std::string& op) const;
};

struct LabelStats {
    uint32_t count = 0;
    std::unordered_map<std::string, EquiDepthHistogram> numeric_histograms;
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> string_cardinalities;
};

class GraphStatistics {
private:
    std::unordered_map<std::string, LabelStats> node_stats_;
    std::unordered_map<std::string, uint32_t> edge_type_counts_;
    uint32_t total_nodes_ = 0;
    uint32_t total_edges_ = 0;

public:
    GraphStatistics() = default;
    ~GraphStatistics() = default;

    void analyze(GraphEngine& ge);
    double estimate_node_selectivity(const std::string& label, const std::string& key, const std::string& op, const Variant& value) const;
    double estimate_edge_selectivity(const std::string& type) const;
    uint32_t get_total_nodes() const { return total_nodes_; }
    uint32_t get_total_edges() const { return total_edges_; }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_STATISTICS_H
