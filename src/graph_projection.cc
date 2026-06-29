#include "graph_projection.h"

namespace AetherGraph {

GraphProjection::GraphProjection(GraphEngine& base_ge) : base_ge_(base_ge) {}

void GraphProjection::project_by_label(const std::string& node_label) {
    const auto& nodes = base_ge_.get_all_nodes();
    for (const auto& [nid, n] : nodes) {
        if (n->label == node_label) {
            projected_nodes_.insert(nid);
        }
    }

    // Include edges where both src and dest are projected
    const auto& edges = base_ge_.get_all_edges();
    for (const auto& [eid, e] : edges) {
        if (projected_nodes_.count(e->src_id) > 0 && projected_nodes_.count(e->dest_id) > 0) {
            projected_edges_.insert(eid);
        }
    }
}

void GraphProjection::project_by_edge_type(const std::string& edge_type) {
    const auto& edges = base_ge_.get_all_edges();
    for (const auto& [eid, e] : edges) {
        if (e->type == edge_type) {
            projected_edges_.insert(eid);
            projected_nodes_.insert(e->src_id);
            projected_nodes_.insert(e->dest_id);
        }
    }
}

void GraphProjection::project_by_node_property(const std::string& key, const Variant& val) {
    const auto& nodes = base_ge_.get_all_nodes();
    for (const auto& [nid, n] : nodes) {
        auto it = n->properties.find(key);
        if (it != n->properties.end() && it->second == val) {
            projected_nodes_.insert(nid);
        }
    }

    // Include edges connecting matching nodes
    const auto& edges = base_ge_.get_all_edges();
    for (const auto& [eid, e] : edges) {
        if (projected_nodes_.count(e->src_id) > 0 && projected_nodes_.count(e->dest_id) > 0) {
            projected_edges_.insert(eid);
        }
    }
}

void GraphProjection::intersect_projection(const GraphProjection& other) {
    std::unordered_set<node_id_t> nodes_intersect;
    for (node_id_t nid : projected_nodes_) {
        if (other.has_node(nid)) {
            nodes_intersect.insert(nid);
        }
    }
    projected_nodes_ = std::move(nodes_intersect);

    std::unordered_set<edge_id_t> edges_intersect;
    for (edge_id_t eid : projected_edges_) {
        if (other.has_edge(eid)) {
            edges_intersect.insert(eid);
        }
    }
    projected_edges_ = std::move(edges_intersect);
}

void GraphProjection::union_projection(const GraphProjection& other) {
    auto other_nodes = other.get_projected_nodes();
    projected_nodes_.insert(other_nodes.begin(), other_nodes.end());

    auto other_edges = other.get_projected_edges();
    projected_edges_.insert(other_edges.begin(), other_edges.end());
}

std::vector<node_id_t> GraphProjection::get_projected_nodes() const {
    return std::vector<node_id_t>(projected_nodes_.begin(), projected_nodes_.end());
}

std::vector<edge_id_t> GraphProjection::get_projected_edges() const {
    return std::vector<edge_id_t>(projected_edges_.begin(), projected_edges_.end());
}

} // namespace AetherGraph
