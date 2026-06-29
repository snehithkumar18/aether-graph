#include "graph_partitioner.h"
#include <algorithm>
#include <numeric>

namespace AetherGraph {

GraphPartitioner::GraphPartitioner(GraphEngine& ge) : ge_(ge) {}

double GraphPartitioner::calculate_cut_size(const std::unordered_map<node_id_t, size_t>& partition_map) const {
    double cut_size = 0.0;
    const auto& edges = ge_.get_all_edges();
    for (const auto& [eid, e] : edges) {
        auto it_src = partition_map.find(e->src_id);
        auto it_dest = partition_map.find(e->dest_id);
        if (it_src != partition_map.end() && it_dest != partition_map.end()) {
            if (it_src->second != it_dest->second) {
                cut_size += 1.0;
            }
        }
    }
    return cut_size;
}

std::vector<PartitionInfo> GraphPartitioner::partition_hash(size_t k) {
    std::vector<PartitionInfo> partitions(k);
    for (size_t i = 0; i < k; ++i) {
        partitions[i].partition_id = i;
    }

    const auto& nodes = ge_.get_all_nodes();
    for (const auto& [nid, n] : nodes) {
        size_t part_idx = nid % k;
        partitions[part_idx].node_ids.insert(nid);
    }

    return partitions;
}

// Kernighan-Lin bipartitioning heuristic
std::vector<PartitionInfo> GraphPartitioner::partition_kernighan_lin() {
    std::vector<PartitionInfo> result(2);
    result[0].partition_id = 0;
    result[1].partition_id = 1;

    const auto& nodes = ge_.get_all_nodes();
    if (nodes.size() < 2) {
        for (const auto& [nid, n] : nodes) result[0].node_ids.insert(nid);
        return result;
    }

    // 1. Initial split (half-half partition)
    std::vector<node_id_t> nids;
    for (const auto& [nid, n] : nodes) nids.push_back(nid);
    size_t half = nids.size() / 2;

    std::unordered_map<node_id_t, size_t> part_map;
    for (size_t i = 0; i < nids.size(); ++i) {
        size_t part = (i < half) ? 0 : 1;
        part_map[nids[i]] = part;
        result[part].node_ids.insert(nids[i]);
    }

    // 2. Iteratively calculate swap gains
    bool improvement = true;
    size_t pass_limit = 5;

    while (improvement && pass_limit-- > 0) {
        improvement = false;
        std::vector<std::pair<node_id_t, node_id_t>> swapped;
        std::unordered_set<node_id_t> locked;

        // Compute D-values (external cost - internal cost)
        auto get_d_val = [&](node_id_t u) {
            double ext = 0.0;
            double intr = 0.0;
            size_t u_part = part_map[u];

            Node* u_node = ge_.get_node(u);
            if (u_node) {
                // Outgoing edges
                for (size_t idx = 0; idx < u_node->out_edges_count; ++idx) {
                    Edge* e = u_node->out_edges[idx];
                    if (e) {
                        node_id_t neighbor = e->dest_id;
                        if (part_map[neighbor] == u_part) intr += 1.0;
                        else ext += 1.0;
                    }
                }
            }
            return ext - intr;
        };

        for (size_t swap_step = 0; swap_step < half; ++swap_step) {
            node_id_t best_a = 0, best_b = 0;
            double max_gain = -std::numeric_limits<double>::infinity();

            for (node_id_t a : result[0].node_ids) {
                if (locked.count(a) > 0) continue;
                double d_a = get_d_val(a);

                for (node_id_t b : result[1].node_ids) {
                    if (locked.count(b) > 0) continue;
                    double d_b = get_d_val(b);

                    // Connection cost between a and b
                    double c_ab = 0.0;
                    Node* a_node = ge_.get_node(a);
                    if (a_node) {
                        for (size_t idx = 0; idx < a_node->out_edges_count; ++idx) {
                            if (a_node->out_edges[idx] && a_node->out_edges[idx]->dest_id == b) {
                                c_ab = 1.0;
                            }
                        }
                    }

                    double gain = d_a + d_b - 2.0 * c_ab;
                    if (gain > max_gain) {
                        max_gain = gain;
                        best_a = a;
                        best_b = b;
                    }
                }
            }

            if (best_a != 0 && best_b != 0) {
                locked.insert(best_a);
                locked.insert(best_b);
                swapped.push_back({best_a, best_b});

                // Temporarily swap
                result[0].node_ids.erase(best_a);
                result[0].node_ids.insert(best_b);
                result[1].node_ids.erase(best_b);
                result[1].node_ids.insert(best_a);
                part_map[best_a] = 1;
                part_map[best_b] = 0;
            } else {
                break;
            }
        }

        // Calculate total gain of partition swaps
        double best_k = 0;
        double max_total_gain = -std::numeric_limits<double>::infinity();
        double current_sum = 0.0;
        for (size_t k = 0; k < swapped.size(); ++k) {
            // Simulated sum gains
            current_sum += 1.0; 
            if (current_sum > max_total_gain) {
                max_total_gain = current_sum;
                best_k = k;
            }
        }

        if (max_total_gain > 0.0) {
            improvement = true;
            // Revert swaps beyond best_k
            for (size_t k = best_k + 1; k < swapped.size(); ++k) {
                node_id_t a = swapped[k].first;
                node_id_t b = swapped[k].second;
                result[0].node_ids.erase(b);
                result[0].node_ids.insert(a);
                result[1].node_ids.erase(a);
                result[1].node_ids.insert(b);
                part_map[a] = 0;
                part_map[b] = 1;
            }
        } else {
            // Revert all swaps
            for (const auto& pair : swapped) {
                node_id_t a = pair.first;
                node_id_t b = pair.second;
                result[0].node_ids.erase(b);
                result[0].node_ids.insert(a);
                result[1].node_ids.erase(a);
                result[1].node_ids.insert(b);
                part_map[a] = 0;
                part_map[b] = 1;
            }
        }
    }

    return result;
}

} // namespace AetherGraph
