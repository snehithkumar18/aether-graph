#ifndef AETHER_GRAPH_GRAPH_PARTITIONER_H
#define AETHER_GRAPH_GRAPH_PARTITIONER_H

#include "graph_engine.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace AetherGraph {

struct PartitionInfo {
    size_t partition_id;
    std::unordered_set<node_id_t> node_ids;
};

class GraphPartitioner {
private:
    GraphEngine& ge_;

    double calculate_cut_size(const std::unordered_map<node_id_t, size_t>& partition_map) const;

public:
    explicit GraphPartitioner(GraphEngine& ge);
    ~GraphPartitioner() = default;

    // Range hash partitioner
    std::vector<PartitionInfo> partition_hash(size_t k);

    // Kernighan-Lin heuristic bipartitioning
    std::vector<PartitionInfo> partition_kernighan_lin();
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_GRAPH_PARTITIONER_H
