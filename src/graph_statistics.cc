#include "graph_statistics.h"
#include <algorithm>

namespace AetherGraph {

void EquiDepthHistogram::build(const std::vector<float>& values, size_t num_buckets) {
    buckets_.clear();
    total_count_ = static_cast<uint32_t>(values.size());
    if (values.empty()) return;

    std::vector<float> sorted_vals = values;
    std::sort(sorted_vals.begin(), sorted_vals.end());

    if (num_buckets == 0) num_buckets = 1;
    size_t vals_per_bucket = values.size() / num_buckets;
    if (vals_per_bucket == 0) vals_per_bucket = 1;

    for (size_t i = 0; i < sorted_vals.size(); i += vals_per_bucket) {
        Bucket b;
        b.lower_bound = sorted_vals[i];
        size_t end_idx = std::min(i + vals_per_bucket - 1, sorted_vals.size() - 1);
        b.upper_bound = sorted_vals[end_idx];
        b.count = static_cast<uint32_t>(end_idx - i + 1);
        buckets_.push_back(b);
    }
}

double EquiDepthHistogram::estimate_selectivity(float val, const std::string& op) const {
    if (buckets_.empty()) return 0.0;

    if (op == "=") {
        for (const auto& b : buckets_) {
            if (val >= b.lower_bound && val <= b.upper_bound) {
                // Approximate uniform distribution within the matching bucket
                double range = b.upper_bound - b.lower_bound;
                if (range == 0.0) return static_cast<double>(b.count) / total_count_;
                return (1.0 / range) / total_count_;
            }
        }
        return 0.0;
    }

    if (op == "<" || op == "<=") {
        uint32_t cumulative = 0;
        for (const auto& b : buckets_) {
            if (val > b.upper_bound) {
                cumulative += b.count;
            } else if (val >= b.lower_bound && val <= b.upper_bound) {
                double range = b.upper_bound - b.lower_bound;
                if (range == 0.0) {
                    cumulative += b.count;
                } else {
                    double fraction = (val - b.lower_bound) / range;
                    cumulative += static_cast<uint32_t>(fraction * b.count);
                }
                break;
            } else {
                break; // val < b.lower_bound
            }
        }
        return static_cast<double>(cumulative) / total_count_;
    }

    if (op == ">" || op == ">=") {
        uint32_t cumulative = 0;
        for (auto rit = buckets_.rbegin(); rit != buckets_.rend(); ++rit) {
            const auto& b = *rit;
            if (val < b.lower_bound) {
                cumulative += b.count;
            } else if (val >= b.lower_bound && val <= b.upper_bound) {
                double range = b.upper_bound - b.lower_bound;
                if (range == 0.0) {
                    cumulative += b.count;
                } else {
                    double fraction = (b.upper_bound - val) / range;
                    cumulative += static_cast<uint32_t>(fraction * b.count);
                }
                break;
            } else {
                break; // val > b.upper_bound
            }
        }
        return static_cast<double>(cumulative) / total_count_;
    }

    return 0.5; // Default estimate
}

// ======================================================================
// GraphStatistics Implementation
// ======================================================================
void GraphStatistics::analyze(GraphEngine& ge) {
    node_stats_.clear();
    edge_type_counts_.clear();
    total_nodes_ = 0;
    total_edges_ = 0;

    const auto& nodes = ge.get_all_nodes();
    const auto& edges = ge.get_all_edges();

    total_nodes_ = static_cast<uint32_t>(nodes.size());
    total_edges_ = static_cast<uint32_t>(edges.size());

    // Gather numeric and categorical values grouped by node labels
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<float>>> numeric_lists;

    for (const auto& [nid, n] : nodes) {
        auto& l_stats = node_stats_[n->label];
        l_stats.count++;

        for (const auto& [k, v] : n->properties) {
            if (v.type == DataType::INT) {
                numeric_lists[n->label][k].push_back(static_cast<float>(v.get_int()));
            } else if (v.type == DataType::FLOAT) {
                numeric_lists[n->label][k].push_back(v.get_float());
            } else if (v.type == DataType::STRING) {
                l_stats.string_cardinalities[k][v.get_string()]++;
            }
        }
    }

    // Build equi-depth histograms
    for (auto& [label, prop_map] : numeric_lists) {
        auto& l_stats = node_stats_[label];
        for (const auto& [prop_name, vals] : prop_map) {
            l_stats.numeric_histograms[prop_name].build(vals, 10);
        }
    }

    // Count edge type distribution
    for (const auto& [eid, e] : edges) {
        edge_type_counts_[e->type]++;
    }
}

double GraphStatistics::estimate_node_selectivity(
    const std::string& label, const std::string& key, const std::string& op, const Variant& value) const {

    auto it = node_stats_.find(label);
    if (it == node_stats_.end()) return 0.0;
    const auto& l_stats = it->second;

    if (value.type == DataType::INT || value.type == DataType::FLOAT) {
        float val = (value.type == DataType::INT) ? static_cast<float>(value.get_int()) : value.get_float();
        auto hist_it = l_stats.numeric_histograms.find(key);
        if (hist_it != l_stats.numeric_histograms.end()) {
            return hist_it->second.estimate_selectivity(val, op);
        }
    } else if (value.type == DataType::STRING) {
        std::string s_val = value.get_string();
        auto card_it = l_stats.string_cardinalities.find(key);
        if (card_it != l_stats.string_cardinalities.end()) {
            const auto& counts = card_it->second;
            auto val_it = counts.find(s_val);
            if (val_it != counts.end()) {
                double freq = static_cast<double>(val_it->second) / l_stats.count;
                if (op == "=") return freq;
                if (op == "!=") return 1.0 - freq;
            }
        }
    }

    return 0.1; // Default fallback
}

double GraphStatistics::estimate_edge_selectivity(const std::string& type) const {
    if (total_edges_ == 0) return 0.0;
    auto it = edge_type_counts_.find(type);
    if (it != edge_type_counts_.end()) {
        return static_cast<double>(it->second) / total_edges_;
    }
    return 0.0;
}

} // namespace AetherGraph
