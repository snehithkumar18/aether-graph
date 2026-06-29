#ifndef AETHER_GRAPH_VECTOR_INDEX_H
#define AETHER_GRAPH_VECTOR_INDEX_H

#include "graph_engine.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace AetherGraph {

struct VectorSearchResult {
    node_id_t node_id;
    float distance;
};

class VectorIndex {
private:
    std::string property_key_;
    size_t dimension_;
    
    // Flat storage for IVF-FLAT style search
    std::vector<std::pair<node_id_t, std::vector<float>>> vectors_;
    
    // Cluster centroids for indexing partitions
    std::vector<std::vector<float>> centroids_;
    std::unordered_map<size_t, std::vector<size_t>> cluster_buckets_; // centroid_idx -> index in vectors_

    float calculate_l2_distance(const std::vector<float>& v1, const std::vector<float>& v2) const;
    float calculate_cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) const;

public:
    VectorIndex(const std::string& property_key, size_t dimension);
    ~VectorIndex() = default;

    void add_vector(node_id_t node_id, const std::vector<float>& vec);
    void build_index(size_t num_clusters = 4);
    
    std::vector<VectorSearchResult> search_knn(const std::vector<float>& query_vec, size_t k, const std::string& metric = "l2");
    void build_from_graph(GraphEngine& ge);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_VECTOR_INDEX_H
