#include "graph_algorithms.h"
#include <queue>
#include <set>
#include <stack>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace AetherGraph {

// ======================================================================
// PageRank Implementation
// ======================================================================
std::unordered_map<node_id_t, double> GraphAlgorithms::page_rank(
    GraphEngine& ge, double damping_factor, int max_iterations, double convergence_threshold) {
    
    const auto& nodes = ge.get_all_nodes();
    size_t num_nodes = nodes.size();
    std::unordered_map<node_id_t, double> ranks;
    if (num_nodes == 0) return ranks;

    double initial_rank = 1.0 / num_nodes;
    for (const auto& [nid, node] : nodes) {
        ranks[nid] = initial_rank;
    }

    std::vector<node_id_t> dangling_nodes;
    for (const auto& [nid, node] : nodes) {
        if (node->out_edges_count == 0) {
            dangling_nodes.push_back(nid);
        }
    }

    for (int iter = 0; iter < max_iterations; ++iter) {
        std::unordered_map<node_id_t, double> next_ranks;
        for (const auto& [nid, node] : nodes) {
            next_ranks[nid] = 0.0;
        }

        double dangling_sum = 0.0;
        for (node_id_t dn : dangling_nodes) {
            dangling_sum += ranks[dn];
        }

        for (const auto& [nid, node] : nodes) {
            double current_rank = ranks[nid];
            if (node->out_edges_count > 0) {
                double share = current_rank / node->out_edges_count;
                for (size_t i = 0; i < node->out_edges_count; ++i) {
                    Edge* edge = node->out_edges[i];
                    if (edge) {
                        next_ranks[edge->dest_id] += share;
                    }
                }
            }
        }

        double constant_term = (1.0 - damping_factor) / num_nodes + (damping_factor * dangling_sum / num_nodes);
        double diff = 0.0;
        for (const auto& [nid, node] : nodes) {
            double new_rank = constant_term + damping_factor * next_ranks[nid];
            diff += std::abs(new_rank - ranks[nid]);
            ranks[nid] = new_rank;
        }

        if (diff < convergence_threshold) {
            break;
        }
    }

    return ranks;
}

// ======================================================================
// Dijkstra Shortest Path Implementation
// ======================================================================
DijkstraResult GraphAlgorithms::dijkstra(
    GraphEngine& ge, node_id_t start_node, node_id_t end_node, const std::string& weight_property) {
    
    DijkstraResult result;
    result.total_weight = -1.0;

    const auto& nodes = ge.get_all_nodes();
    if (nodes.find(start_node) == nodes.end() || nodes.find(end_node) == nodes.end()) {
        return result;
    }

    using PQElement = std::pair<double, node_id_t>;
    std::priority_queue<PQElement, std::vector<PQElement>, std::greater<PQElement>> pq;
    std::unordered_map<node_id_t, double> dist;
    std::unordered_map<node_id_t, node_id_t> prev;

    for (const auto& [nid, node] : nodes) {
        dist[nid] = std::numeric_limits<double>::infinity();
    }

    dist[start_node] = 0.0;
    pq.push({0.0, start_node});

    while (!pq.empty()) {
        auto [d, curr] = pq.top();
        pq.pop();

        if (curr == end_node) {
            result.total_weight = d;
            break;
        }

        if (d > dist[curr]) continue;

        Node* node = ge.get_node(curr);
        if (!node) continue;

        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (!edge) continue;

            double weight = 1.0;
            auto it = edge->properties.find(weight_property);
            if (it != edge->properties.end()) {
                if (it->second.type == DataType::INT) {
                    weight = static_cast<double>(it->second.get_int());
                } else if (it->second.type == DataType::FLOAT) {
                    weight = static_cast<double>(it->second.get_float());
                }
            }

            if (weight < 0.0) weight = 0.0; // Prevent negative cycle issues

            double new_dist = d + weight;
            if (new_dist < dist[edge->dest_id]) {
                dist[edge->dest_id] = new_dist;
                prev[edge->dest_id] = curr;
                pq.push({new_dist, edge->dest_id});
            }
        }
    }

    if (dist[end_node] != std::numeric_limits<double>::infinity()) {
        node_id_t curr = end_node;
        while (curr != start_node) {
            result.path.push_back(curr);
            curr = prev[curr];
        }
        result.path.push_back(start_node);
        std::reverse(result.path.begin(), result.path.end());
    }

    return result;
}

// ======================================================================
// Connected Components (Union-Find)
// ======================================================================
std::unordered_map<node_id_t, node_id_t> GraphAlgorithms::connected_components(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    std::unordered_map<node_id_t, node_id_t> parent;
    
    for (const auto& [nid, node] : nodes) {
        parent[nid] = nid;
    }

    auto find_root = [&](node_id_t id, auto& self_fn) -> node_id_t {
        if (parent[id] == id) return id;
        return parent[id] = self_fn(parent[id], self_fn);
    };

    auto union_nodes = [&](node_id_t id1, node_id_t id2) {
        node_id_t root1 = find_root(id1, find_root);
        node_id_t root2 = find_root(id2, find_root);
        if (root1 != root2) {
            parent[root1] = root2;
        }
    };

    for (const auto& [nid, node] : nodes) {
        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (edge) {
                union_nodes(node->id, edge->dest_id);
            }
        }
    }

    std::unordered_map<node_id_t, node_id_t> components;
    for (const auto& [nid, node] : nodes) {
        components[nid] = find_root(nid, find_root);
    }

    return components;
}

// ======================================================================
// Betweenness Centrality
// ======================================================================
std::unordered_map<node_id_t, double> GraphAlgorithms::betweenness_centrality(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    std::unordered_map<node_id_t, double> cb;
    for (const auto& [nid, node] : nodes) {
        cb[nid] = 0.0;
    }

    for (const auto& [s_nid, s_node] : nodes) {
        std::stack<node_id_t> S;
        std::unordered_map<node_id_t, std::vector<node_id_t>> P;
        std::unordered_map<node_id_t, double> sigma;
        std::unordered_map<node_id_t, int> d;

        for (const auto& [t_nid, t_node] : nodes) {
            sigma[t_nid] = 0.0;
            d[t_nid] = -1;
        }

        sigma[s_nid] = 1.0;
        d[s_nid] = 0;

        std::queue<node_id_t> Q;
        Q.push(s_nid);

        while (!Q.empty()) {
            node_id_t v = Q.front();
            Q.pop();
            S.push(v);

            Node* v_node = ge.get_node(v);
            if (!v_node) continue;

            for (size_t i = 0; i < v_node->out_edges_count; ++i) {
                Edge* edge = v_node->out_edges[i];
                if (!edge) continue;
                node_id_t w = edge->dest_id;

                if (d[w] < 0) {
                    Q.push(w);
                    d[w] = d[v] + 1;
                }

                if (d[w] == d[v] + 1) {
                    sigma[w] += sigma[v];
                    P[w].push_back(v);
                }
            }
        }

        std::unordered_map<node_id_t, double> delta;
        for (const auto& [t_nid, t_node] : nodes) {
            delta[t_nid] = 0.0;
        }

        while (!S.empty()) {
            node_id_t w = S.top();
            S.pop();

            for (node_id_t v : P[w]) {
                delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w]);
            }

            if (w != s_nid) {
                cb[w] += delta[w];
            }
        }
    }

    return cb;
}

// ======================================================================
// Louvain Communities Implementation
// ======================================================================
std::unordered_map<node_id_t, node_id_t> GraphAlgorithms::louvain_communities(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    std::unordered_map<node_id_t, node_id_t> community;
    if (nodes.empty()) return community;

    for (const auto& [nid, node] : nodes) {
        community[nid] = nid;
    }

    std::unordered_map<node_id_t, double> k;
    double m = 0.0;

    for (const auto& [nid, node] : nodes) {
        double deg = static_cast<double>(node->out_edges_count);
        k[nid] = deg;
        m += deg;
    }
    m /= 2.0;
    if (m <= 0.0) return community;

    std::unordered_map<node_id_t, double> Tot;
    for (const auto& [nid, node] : nodes) {
        Tot[nid] = k[nid];
    }

    bool improvement = true;
    int loop_limit = 10;
    
    while (improvement && loop_limit-- > 0) {
        improvement = false;
        for (const auto& [nid, node] : nodes) {
            node_id_t c_old = community[nid];
            
            std::unordered_map<node_id_t, double> neighbor_communities;
            for (size_t i = 0; i < node->out_edges_count; ++i) {
                Edge* edge = node->out_edges[i];
                if (edge) {
                    node_id_t neighbor_comm = community[edge->dest_id];
                    neighbor_communities[neighbor_comm] += 1.0;
                }
            }

            node_id_t best_comm = c_old;
            double max_delta_q = 0.0;

            Tot[c_old] -= k[nid];

            for (const auto& [comm, ki_in] : neighbor_communities) {
                double delta_q = ki_in - (Tot[comm] * k[nid]) / m;
                if (delta_q > max_delta_q) {
                    max_delta_q = delta_q;
                    best_comm = comm;
                }
            }

            community[nid] = best_comm;
            Tot[best_comm] += k[nid];

            if (best_comm != c_old) {
                improvement = true;
            }
        }
    }

    return community;
}

// ======================================================================
// Topological Sort (Kahn's Algorithm)
// ======================================================================
std::vector<node_id_t> GraphAlgorithms::topological_sort(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    std::unordered_map<node_id_t, int> in_degree;
    for (const auto& [nid, node] : nodes) {
        in_degree[nid] = 0;
    }

    for (const auto& [nid, node] : nodes) {
        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (edge) {
                in_degree[edge->dest_id]++;
            }
        }
    }

    std::queue<node_id_t> q;
    for (const auto& [nid, node] : nodes) {
        if (in_degree[nid] == 0) {
            q.push(nid);
        }
    }

    std::vector<node_id_t> order;
    while (!q.empty()) {
        node_id_t curr = q.front();
        q.pop();
        order.push_back(curr);

        Node* node = ge.get_node(curr);
        if (!node) continue;

        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (edge) {
                node_id_t dest = edge->dest_id;
                in_degree[dest]--;
                if (in_degree[dest] == 0) {
                    q.push(dest);
                }
            }
        }
    }

    if (order.size() != nodes.size()) {
        return {}; // Cycle detected, not a DAG
    }

    return order;
}

// ======================================================================
// Triangle Counting
// ======================================================================
std::unordered_map<node_id_t, uint32_t> GraphAlgorithms::count_triangles(GraphEngine& ge, uint32_t& total_triangles) {
    const auto& nodes = ge.get_all_nodes();
    std::unordered_map<node_id_t, uint32_t> triangle_counts;
    total_triangles = 0;

    for (const auto& [nid, node] : nodes) {
        triangle_counts[nid] = 0;
    }

    std::unordered_map<node_id_t, std::set<node_id_t>> adj;
    for (const auto& [nid, node] : nodes) {
        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (edge) {
                adj[nid].insert(edge->dest_id);
                adj[edge->dest_id].insert(nid); // Treat as undirected
            }
        }
    }

    for (const auto& [nid, neighbors] : adj) {
        std::vector<node_id_t> neighbor_vec(neighbors.begin(), neighbors.end());
        size_t n_count = neighbor_vec.size();

        for (size_t i = 0; i < n_count; ++i) {
            for (size_t j = i + 1; j < n_count; ++j) {
                node_id_t u = neighbor_vec[i];
                node_id_t v = neighbor_vec[j];
                if (adj[u].count(v) > 0) {
                    triangle_counts[nid]++;
                }
            }
        }
    }

    uint32_t sum = 0;
    for (const auto& [nid, count] : triangle_counts) {
        sum += count;
    }
    total_triangles = sum / 3;

    return triangle_counts;
}

// ======================================================================
// k-Core Decomposition
// ======================================================================
std::unordered_map<node_id_t, uint32_t> GraphAlgorithms::k_core_decomposition(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    std::unordered_map<node_id_t, uint32_t> degrees;
    std::unordered_map<node_id_t, std::vector<node_id_t>> adj;

    for (const auto& [nid, node] : nodes) {
        adj[nid] = std::vector<node_id_t>();
    }

    for (const auto& [nid, node] : nodes) {
        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (edge) {
                adj[nid].push_back(edge->dest_id);
                adj[edge->dest_id].push_back(nid); // Undirected representation
            }
        }
    }

    for (const auto& [nid, neighbors] : adj) {
        degrees[nid] = static_cast<uint32_t>(neighbors.size());
    }

    std::unordered_map<node_id_t, uint32_t> core;
    std::set<std::pair<uint32_t, node_id_t>> deg_set;

    for (const auto& [nid, deg] : degrees) {
        deg_set.insert({deg, nid});
    }

    uint32_t current_k = 0;
    while (!deg_set.empty()) {
        auto [deg, v] = *deg_set.begin();
        deg_set.erase(deg_set.begin());

        current_k = std::max(current_k, deg);
        core[v] = current_k;

        for (node_id_t neighbor : adj[v]) {
            if (core.find(neighbor) == core.end()) {
                uint32_t n_deg = degrees[neighbor];
                deg_set.erase({n_deg, neighbor});
                degrees[neighbor]--;
                deg_set.insert({degrees[neighbor], neighbor});
            }
        }
    }

    return core;
}

} // namespace AetherGraph
