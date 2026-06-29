#ifndef AETHER_GRAPH_GRAPH_PROJECTION_H
#define AETHER_GRAPH_GRAPH_PROJECTION_H

#include "graph_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace AetherGraph {

class GraphProjection {
private:
    GraphEngine& base_ge_;
    std::unordered_set<node_id_t> projected_nodes_;
    std::unordered_set<edge_id_t> projected_edges_;

public:
    explicit GraphProjection(GraphEngine& base_ge);
    ~GraphProjection() = default;

    // Filter node/edge collections into projection
    void project_by_label(const std::string& node_label);
    void project_by_edge_type(const std::string& edge_type);
    void project_by_node_property(const std::string& key, const Variant& val);

    // Subgraph projection operations
    void intersect_projection(const GraphProjection& other);
    void union_projection(const GraphProjection& other);

    // Queries on projection
    bool has_node(node_id_t nid) const { return projected_nodes_.count(nid) > 0; }
    bool has_edge(edge_id_t eid) const { return projected_edges_.count(eid) > 0; }
    
    std::vector<node_id_t> get_projected_nodes() const;
    std::vector<edge_id_t> get_projected_edges() const;

    size_t node_count() const { return projected_nodes_.size(); }
    size_t edge_count() const { return projected_edges_.size(); }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_PROJECTION_H
