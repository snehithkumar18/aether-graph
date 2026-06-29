#include "graph_coordinator.h"
#include <algorithm>
#include <iostream>

namespace AetherGraph {

GraphCoordinator::GraphCoordinator(GraphEngine& ge, uint32_t local_cluster_node_id)
    : ge_(ge), local_cluster_node_id_(local_cluster_node_id) {}

void GraphCoordinator::register_cluster_node(uint32_t node_id, const std::string& ip_address, bool is_leader) {
    auto it = std::find_if(cluster_nodes_.begin(), cluster_nodes_.end(), [node_id](const ClusterNodeInfo& info) {
        return info.node_id == node_id;
    });

    if (it == cluster_nodes_.end()) {
        cluster_nodes_.push_back({node_id, ip_address, is_leader, true});
    } else {
        it->ip_address = ip_address;
        it->is_leader = is_leader;
        it->is_healthy = true;
    }
}

void GraphCoordinator::update_heartbeat(uint32_t node_id, bool is_healthy) {
    auto it = std::find_if(cluster_nodes_.begin(), cluster_nodes_.end(), [node_id](ClusterNodeInfo& info) {
        return info.node_id == node_id;
    });
    if (it != cluster_nodes_.end()) {
        it->is_healthy = is_healthy;
    }
}

uint32_t GraphCoordinator::route_node_query(node_id_t nid) const {
    auto it = node_to_cluster_map_.find(nid);
    if (it != node_to_cluster_map_.end()) {
        return it->second;
    }
    // Fallback: Default to leader node
    for (const auto& node : cluster_nodes_) {
        if (node.is_leader && node.is_healthy) {
            return node.node_id;
        }
    }
    return local_cluster_node_id_;
}

bool GraphCoordinator::distribute_graph_partitions(GraphPartitioner& partitioner) {
    size_t active_node_count = 0;
    std::vector<uint32_t> healthy_ids;
    for (const auto& node : cluster_nodes_) {
        if (node.is_healthy) {
            active_node_count++;
            healthy_ids.push_back(node.node_id);
        }
    }

    if (active_node_count == 0) return false;

    // Run partition algorithm
    auto partitions = partitioner.partition_hash(active_node_count);
    
    node_to_cluster_map_.clear();
    for (size_t i = 0; i < partitions.size(); ++i) {
        uint32_t target_node_id = healthy_ids[i];
        for (node_id_t nid : partitions[i].node_ids) {
            node_to_cluster_map_[nid] = target_node_id;
        }
    }

    return true;
}

bool GraphCoordinator::migrate_node(node_id_t nid, uint32_t target_cluster_node_id) {
    auto it = std::find_if(cluster_nodes_.begin(), cluster_nodes_.end(), [target_cluster_node_id](const ClusterNodeInfo& info) {
        return info.node_id == target_cluster_node_id && info.is_healthy;
    });

    if (it == cluster_nodes_.end()) return false; // Target node not ready

    node_to_cluster_map_[nid] = target_cluster_node_id;
    std::cout << "[COORDINATOR] Migrated node " << nid << " to cluster node " << target_cluster_node_id << ".\n";
    return true;
}

std::vector<node_id_t> GraphCoordinator::get_local_nodes() const {
    std::vector<node_id_t> locals;
    for (const auto& [nid, cluster_id] : node_to_cluster_map_) {
        if (cluster_id == local_cluster_node_id_) {
            locals.push_back(nid);
        }
    }
    return locals;
}

} // namespace AetherGraph
