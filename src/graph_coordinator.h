#ifndef AETHER_GRAPH_GRAPH_COORDINATOR_H
#define AETHER_GRAPH_GRAPH_COORDINATOR_H

#include "graph_engine.h"
#include "graph_partitioner.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace AetherGraph {

struct ClusterNodeInfo {
    uint32_t node_id;
    std::string ip_address;
    bool is_leader;
    bool is_healthy;
};

class GraphCoordinator {
private:
    GraphEngine& ge_;
    std::vector<ClusterNodeInfo> cluster_nodes_;
    std::unordered_map<node_id_t, uint32_t> node_to_cluster_map_; // node ID -> Cluster Node ID
    uint32_t local_cluster_node_id_;

public:
    GraphCoordinator(GraphEngine& ge, uint32_t local_cluster_node_id);
    ~GraphCoordinator() = default;

    void register_cluster_node(uint32_t node_id, const std::string& ip_address, bool is_leader);
    void update_heartbeat(uint32_t node_id, bool is_healthy);

    // Distributed query routing
    uint32_t route_node_query(node_id_t nid) const;
    bool distribute_graph_partitions(GraphPartitioner& partitioner);

    // Partition migration simulating
    bool migrate_node(node_id_t nid, uint32_t target_cluster_node_id);
    
    std::vector<node_id_t> get_local_nodes() const;
    const std::vector<ClusterNodeInfo>& get_cluster_nodes() const { return cluster_nodes_; }
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_COORDINATOR_H
