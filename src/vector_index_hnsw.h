#ifndef AETHER_GRAPH_VECTOR_INDEX_HNSW_H
#define AETHER_GRAPH_VECTOR_INDEX_HNSW_H

#include "utils.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace AetherGraph {

struct HNSWNode {
    node_id_t node_id;
    std::vector<float> vector;
    std::vector<std::vector<node_id_t>> neighbors; // Neighbors per layer
};

class VectorIndexHNSW {
private:
    size_t dimension_;
    size_t max_elements_;
    size_t M_;
    size_t ef_construction_;
    size_t ef_search_;
    node_id_t enter_node_ = -1;
    int max_level_ = -1;

    std::unordered_map<node_id_t, HNSWNode> nodes_;
    std::mutex mutex_;

    double calculate_distance(const std::vector<float>& v1, const std::vector<float>& v2);

public:
    VectorIndexHNSW(size_t dimension, size_t max_elements, size_t M = 16, size_t ef_construction = 200, size_t ef_search = 50);
    ~VectorIndexHNSW() = default;

    void insert(node_id_t node_id, const std::vector<float>& vector);
    std::vector<node_id_t> search_knn(const std::vector<float>& query, size_t k);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_VECTOR_INDEX_HNSW_H
