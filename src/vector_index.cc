#include "vector_index.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <queue>

namespace AetherGraph {

VectorIndex::VectorIndex(const std::string& property_key, size_t dimension)
    : property_key_(property_key), dimension_(dimension) {}

float VectorIndex::calculate_l2_distance(const std::vector<float>& v1, const std::vector<float>& v2) const {
    if (v1.size() != v2.size()) return std::numeric_limits<float>::max();
    float sum = 0.0f;
    for (size_t i = 0; i < v1.size(); ++i) {
        float diff = v1[i] - v2[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

float VectorIndex::calculate_cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) const {
    if (v1.size() != v2.size() || v1.empty()) return -1.0f;
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    for (size_t i = 0; i < v1.size(); ++i) {
        dot += v1[i] * v2[i];
        norm_a += v1[i] * v1[i];
        norm_b += v2[i] * v2[i];
    }
    if (norm_a == 0.0f || norm_b == 0.0f) return -1.0f;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

void VectorIndex::add_vector(node_id_t node_id, const std::vector<float>& vec) {
    if (vec.size() == dimension_) {
        vectors_.push_back({node_id, vec});
    }
}

// IVF-FLAT style index building using simple K-Means clustering
void VectorIndex::build_index(size_t num_clusters) {
    centroids_.clear();
    cluster_buckets_.clear();
    if (vectors_.size() < num_clusters || num_clusters == 0) return;

    // 1. Initialize centroids randomly from vectors
    std::mt19937 rng(42);
    std::vector<size_t> indices(vectors_.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    for (size_t i = 0; i < num_clusters; ++i) {
        centroids_.push_back(vectors_[indices[i]].second);
    }

    // 2. Run K-Means refinement loops
    size_t max_iters = 10;
    for (size_t iter = 0; iter < max_iters; ++iter) {
        cluster_buckets_.clear();

        // Assign vectors to nearest centroid
        for (size_t i = 0; i < vectors_.size(); ++i) {
            size_t best_c = 0;
            float min_dist = std::numeric_limits<float>::max();
            for (size_t c = 0; c < num_clusters; ++c) {
                float dist = calculate_l2_distance(vectors_[i].second, centroids_[c]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_c = c;
                }
            }
            cluster_buckets_[best_c].push_back(i);
        }

        // Recompute centroids as mean of assigned vectors
        for (size_t c = 0; c < num_clusters; ++c) {
            const auto& bucket = cluster_buckets_[c];
            if (bucket.empty()) continue;

            std::vector<float> mean(dimension_, 0.0f);
            for (size_t idx : bucket) {
                for (size_t d = 0; d < dimension_; ++d) {
                    mean[d] += vectors_[idx].second[d];
                }
            }
            for (size_t d = 0; d < dimension_; ++d) {
                mean[d] /= bucket.size();
            }
            centroids_[c] = mean;
        }
    }
}

std::vector<VectorSearchResult> VectorIndex::search_knn(
    const std::vector<float>& query_vec, size_t k, const std::string& metric) {

    std::vector<VectorSearchResult> all_results;
    if (query_vec.size() != dimension_) return all_results;

    // Search nearest centroids first to prune partitions (IVF partition search)
    std::vector<size_t> target_centroids;
    if (!centroids_.empty()) {
        using CentroidDist = std::pair<float, size_t>;
        std::priority_queue<CentroidDist, std::vector<CentroidDist>, std::greater<CentroidDist>> pq;
        for (size_t c = 0; c < centroids_.size(); ++c) {
            pq.push({calculate_l2_distance(query_vec, centroids_[c]), c});
        }
        
        // Search the top 2 closest partitions
        for (size_t step = 0; step < 2 && !pq.empty(); ++step) {
            target_centroids.push_back(pq.top().second);
            pq.pop();
        }
    }

    // Perform distance calculation inside target partitions
    auto scan_bucket = [&](const std::vector<size_t>& vec_indices) {
        for (size_t idx : vec_indices) {
            const auto& [nid, vec] = vectors_[idx];
            float dist = 0.0f;
            if (metric == "l2") {
                dist = calculate_l2_distance(query_vec, vec);
            } else if (metric == "cosine") {
                dist = 1.0f - calculate_cosine_similarity(query_vec, vec); // Cosine distance
            }
            all_results.push_back({nid, dist});
        }
    };

    if (target_centroids.empty()) {
        // Fallback: Full sequential flat scan
        std::vector<size_t> all_indices(vectors_.size());
        std::iota(all_indices.begin(), all_indices.end(), 0);
        scan_bucket(all_indices);
    } else {
        for (size_t c : target_centroids) {
            scan_bucket(cluster_buckets_[c]);
        }
    }

    // Sort top k results
    std::sort(all_results.begin(), all_results.end(), [](const auto& a, const auto& b) {
        return a.distance < b.distance;
    });

    if (all_results.size() > k) {
        all_results.resize(k);
    }

    return all_results;
}

void VectorIndex::build_from_graph(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    for (const auto& [nid, n] : nodes) {
        auto it = n->properties.find(property_key_);
        if (it != n->properties.end() && it->second.type == DataType::VECTOR) {
            add_vector(n->id, it->second.get_vector());
        }
    }
    build_index();
}

} // namespace AetherGraph
