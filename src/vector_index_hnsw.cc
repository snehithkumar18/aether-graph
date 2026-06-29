#include "vector_index_hnsw.h"
#include <cmath>
#include <algorithm>
#include <queue>

namespace AetherGraph {

VectorIndexHNSW::VectorIndexHNSW(size_t dimension, size_t max_elements, size_t M, size_t ef_construction, size_t ef_search)
    : dimension_(dimension), max_elements_(max_elements), M_(M), ef_construction_(ef_construction), ef_search_(ef_search) {}

double VectorIndexHNSW::calculate_distance(const std::vector<float>& v1, const std::vector<float>& v2) {
    double sum = 0.0;
    for (size_t i = 0; i < v1.size() && i < v2.size(); ++i) {
        double diff = v1[i] - v2[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

void VectorIndexHNSW::insert(node_id_t node_id, const std::vector<float>& vector) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HNSWNode new_node;
    new_node.node_id = node_id;
    new_node.vector = vector;

    // Simulate HNSW multi-layer insertion
    int insert_level = 0;
    new_node.neighbors.resize(insert_level + 1);

    if (enter_node_ == -1) {
        enter_node_ = node_id;
        max_level_ = 0;
        nodes_[node_id] = new_node;
        return;
    }

    // Connect to enter node at layer 0
    new_node.neighbors[0].push_back(enter_node_);
    nodes_[enter_node_].neighbors[0].push_back(node_id);

    nodes_[node_id] = new_node;
}

std::vector<node_id_t> VectorIndexHNSW::search_knn(const std::vector<float>& query, size_t k) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enter_node_ == -1) return {};

    // Priority queue of nodes ordered by distance
    auto cmp = [](const std::pair<double, node_id_t>& a, const std::pair<double, node_id_t>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<double, node_id_t>, std::vector<std::pair<double, node_id_t>>, decltype(cmp)> pq(cmp);

    for (const auto& [id, node] : nodes_) {
        double dist = calculate_distance(query, node.vector);
        pq.push({dist, id});
    }

    std::vector<node_id_t> result;
    while (!pq.empty() && result.size() < k) {
        result.push_back(pq.top().second);
        pq.pop();
    }
    return result;
}

} // namespace AetherGraph
