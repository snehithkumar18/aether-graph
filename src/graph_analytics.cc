#include "graph_analytics.h"
#include <queue>
#include <set>
#include <stack>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <unordered_set>

namespace AetherGraph {

// ======================================================================
// Degree Distribution
// ======================================================================
DegreeStats GraphAnalytics::compute_degree_distribution(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    DegreeStats stats = {0, 0, 0.0, 0.0, 0.0, {}};
    if (nodes.empty()) return stats;

    std::vector<uint32_t> degrees;
    degrees.reserve(nodes.size());

    // Build degree lists (undirected degrees)
    std::unordered_map<node_id_t, uint32_t> counts;
    for (const auto& [nid, node] : nodes) {
        counts[nid] += node->out_edges_count;
        for (size_t i = 0; i < node->out_edges_count; ++i) {
            Edge* edge = node->out_edges[i];
            if (edge) {
                counts[edge->dest_id]++;
            }
        }
    }

    for (const auto& [nid, deg] : counts) {
        degrees.push_back(deg);
    }
    while (degrees.size() < nodes.size()) {
        degrees.push_back(0); // isolated nodes
    }

    std::sort(degrees.begin(), degrees.end());
    stats.min_degree = degrees.front();
    stats.max_degree = degrees.back();

    double sum = std::accumulate(degrees.begin(), degrees.end(), 0.0);
    stats.mean_degree = sum / degrees.size();

    if (degrees.size() % 2 == 0) {
        stats.median_degree = (degrees[degrees.size() / 2 - 1] + degrees[degrees.size() / 2]) / 2.0;
    } else {
        stats.median_degree = degrees[degrees.size() / 2];
    }

    double sq_sum = 0.0;
    for (uint32_t d : degrees) {
        sq_sum += (d - stats.mean_degree) * (d - stats.mean_degree);
    }
    stats.std_dev_degree = std::sqrt(sq_sum / degrees.size());

    // Histogram
    stats.degree_histogram.resize(stats.max_degree + 1, 0);
    for (uint32_t d : degrees) {
        stats.degree_histogram[d]++;
    }

    return stats;
}

// ======================================================================
// Clustering Coefficient
// ======================================================================
double GraphAnalytics::local_clustering_coefficient(GraphEngine& ge, node_id_t node_id) {
    Node* node = ge.get_node(node_id);
    if (!node) return 0.0;

    std::unordered_set<node_id_t> neighbors;
    for (size_t i = 0; i < node->out_edges_count; ++i) {
        Edge* edge = node->out_edges[i];
        if (edge) neighbors.insert(edge->dest_id);
    }

    // Include incoming neighbor relationships
    const auto& nodes = ge.get_all_nodes();
    for (const auto& [nid, n] : nodes) {
        for (size_t i = 0; i < n->out_edges_count; ++i) {
            Edge* edge = n->out_edges[i];
            if (edge && edge->dest_id == node_id) {
                neighbors.insert(nid);
            }
        }
    }

    neighbors.erase(node_id); // remove self
    size_t k = neighbors.size();
    if (k < 2) return 0.0;

    size_t links = 0;
    for (node_id_t u : neighbors) {
        Node* u_node = ge.get_node(u);
        if (!u_node) continue;
        for (size_t i = 0; i < u_node->out_edges_count; ++i) {
            Edge* edge = u_node->out_edges[i];
            if (edge && neighbors.count(edge->dest_id) > 0) {
                links++;
            }
        }
    }

    return static_cast<double>(links) / (k * (k - 1));
}

double GraphAnalytics::global_clustering_coefficient(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    if (nodes.empty()) return 0.0;

    double sum = 0.0;
    for (const auto& [nid, node] : nodes) {
        sum += local_clustering_coefficient(ge, nid);
    }
    return sum / nodes.size();
}

// ======================================================================
// Graph Density
// ======================================================================
GraphDensityInfo GraphAnalytics::compute_density(GraphEngine& ge) {
    GraphDensityInfo info = {0.0, 0, 0, 0.0, true};
    info.node_count = static_cast<uint32_t>(ge.get_all_nodes().size());
    info.edge_count = static_cast<uint32_t>(ge.get_all_edges().size());

    if (info.node_count < 2) {
        info.density = 0.0;
        return info;
    }

    double possible_edges = static_cast<double>(info.node_count) * (info.node_count - 1);
    info.density = static_cast<double>(info.edge_count) / possible_edges;
    info.avg_degree = static_cast<double>(info.edge_count) / info.node_count;
    info.is_sparse = info.density < 0.1;

    return info;
}

// ======================================================================
// Diameter and Radius
// ======================================================================
bool GraphAnalytics::compute_diameter_and_radius(GraphEngine& ge, uint32_t& diameter, uint32_t& radius) {
    const auto& nodes = ge.get_all_nodes();
    if (nodes.empty()) return false;

    diameter = 0;
    radius = std::numeric_limits<uint32_t>::max();

    for (const auto& [s_nid, s_node] : nodes) {
        std::unordered_map<node_id_t, uint32_t> dist;
        std::queue<node_id_t> q;

        dist[s_nid] = 0;
        q.push(s_nid);

        uint32_t max_dist = 0;

        while (!q.empty()) {
            node_id_t u = q.front();
            q.pop();

            uint32_t d = dist[u];
            max_dist = std::max(max_dist, d);

            Node* u_node = ge.get_node(u);
            if (!u_node) continue;

            for (size_t i = 0; i < u_node->out_edges_count; ++i) {
                Edge* edge = u_node->out_edges[i];
                if (edge && dist.find(edge->dest_id) == dist.end()) {
                    dist[edge->dest_id] = d + 1;
                    q.push(edge->dest_id);
                }
            }
        }

        // If the graph is not fully connected, BFS won't reach all nodes.
        // We only compute diameter/radius for connected components.
        if (dist.size() == nodes.size()) {
            diameter = std::max(diameter, max_dist);
            radius = std::min(radius, max_dist);
        }
    }

    if (radius == std::numeric_limits<uint32_t>::max()) {
        radius = 0;
        return false;
    }

    return true;
}

// ======================================================================
// Tarjan's Strongly Connected Components
// ======================================================================
std::vector<std::vector<node_id_t>> GraphAnalytics::strongly_connected_components(GraphEngine& ge) {
    const auto& nodes = ge.get_all_nodes();
    std::vector<std::vector<node_id_t>> sccs;

    std::unordered_map<node_id_t, int> dfn;
    std::unordered_map<node_id_t, int> low;
    std::unordered_map<node_id_t, bool> in_stack;
    std::stack<node_id_t> st;
    int timer = 0;

    auto dfs = [&](node_id_t u, auto& self_fn) -> void {
        dfn[u] = low[u] = ++timer;
        st.push(u);
        in_stack[u] = true;

        Node* u_node = ge.get_node(u);
        if (u_node) {
            for (size_t i = 0; i < u_node->out_edges_count; ++i) {
                Edge* edge = u_node->out_edges[i];
                if (edge) {
                    node_id_t v = edge->dest_id;
                    if (dfn.find(v) == dfn.end()) {
                        self_fn(v, self_fn);
                        low[u] = std::min(low[u], low[v]);
                    } else if (in_stack[v]) {
                        low[u] = std::min(low[u], dfn[v]);
                    }
                }
            }
        }

        if (dfn[u] == low[u]) {
            std::vector<node_id_t> scc;
            while (true) {
                node_id_t curr = st.top();
                st.pop();
                in_stack[curr] = false;
                scc.push_back(curr);
                if (curr == u) break;
            }
            sccs.push_back(scc);
        }
    };

    for (const auto& [nid, n] : nodes) {
        if (dfn.find(nid) == dfn.end()) {
            dfs(nid, dfs);
        }
    }

    return sccs;
}

// ======================================================================
// Graph Isomorphism Check (Simplified VF2 Backtracking)
// ======================================================================
static bool check_feasibility(
    GraphEngine& ge1, GraphEngine& ge2,
    const std::unordered_map<node_id_t, node_id_t>& mapping,
    node_id_t u, node_id_t v) {

    Node* u_node = ge1.get_node(u);
    Node* v_node = ge2.get_node(v);
    if (!u_node || !v_node) return false;
    if (u_node->label != v_node->label) return false;

    // Check outgoing edge mappings
    for (size_t i = 0; i < u_node->out_edges_count; ++i) {
        Edge* edge = u_node->out_edges[i];
        if (edge) {
            auto it = mapping.find(edge->dest_id);
            if (it != mapping.end()) {
                node_id_t mapped_dest = it->second;
                // Verify ge2 has a matching edge
                bool edge_found = false;
                for (size_t j = 0; j < v_node->out_edges_count; ++j) {
                    Edge* v_edge = v_node->out_edges[j];
                    if (v_edge && v_edge->dest_id == mapped_dest && v_edge->type == edge->type) {
                        edge_found = true;
                        break;
                    }
                }
                if (!edge_found) return false;
            }
        }
    }

    return true;
}

static bool vf2_recursive(
    GraphEngine& ge1, GraphEngine& ge2,
    std::unordered_map<node_id_t, node_id_t>& mapping,
    std::unordered_set<node_id_t>& mapped_ge2,
    std::vector<node_id_t>& ge1_nodes,
    std::vector<node_id_t>& ge2_nodes,
    size_t step) {

    if (step == ge1_nodes.size()) return true;

    node_id_t u = ge1_nodes[step];
    for (node_id_t v : ge2_nodes) {
        if (mapped_ge2.find(v) == mapped_ge2.end()) {
            if (check_feasibility(ge1, ge2, mapping, u, v)) {
                mapping[u] = v;
                mapped_ge2.insert(v);

                if (vf2_recursive(ge1, ge2, mapping, mapped_ge2, ge1_nodes, ge2_nodes, step + 1)) {
                    return true;
                }

                mapping.erase(u);
                mapped_ge2.erase(v);
            }
        }
    }

    return false;
}

bool GraphAnalytics::are_isomorphic(GraphEngine& ge1, GraphEngine& ge2) {
    const auto& nodes1 = ge1.get_all_nodes();
    const auto& nodes2 = ge2.get_all_nodes();

    if (nodes1.size() != nodes2.size()) return false;
    if (ge1.get_all_edges().size() != ge2.get_all_edges().size()) return false;

    std::vector<node_id_t> ge1_nodes;
    std::vector<node_id_t> ge2_nodes;

    for (const auto& [nid, n] : nodes1) ge1_nodes.push_back(nid);
    for (const auto& [nid, n] : nodes2) ge2_nodes.push_back(nid);

    std::unordered_map<node_id_t, node_id_t> mapping;
    std::unordered_set<node_id_t> mapped_ge2;

    return vf2_recursive(ge1, ge2, mapping, mapped_ge2, ge1_nodes, ge2_nodes, 0);
}

// ======================================================================
// Random Walk
// ======================================================================
std::vector<node_id_t> GraphAnalytics::random_walk(
    GraphEngine& ge, node_id_t start_node, size_t length, double restart_prob) {

    std::vector<node_id_t> path;
    if (ge.get_all_nodes().find(start_node) == ge.get_all_nodes().end()) return path;

    node_id_t curr = start_node;
    path.push_back(curr);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> dist_real(0.0, 1.0);

    for (size_t step = 1; step < length; ++step) {
        if (dist_real(rng) < restart_prob) {
            curr = start_node;
        } else {
            Node* node = ge.get_node(curr);
            if (node && node->out_edges_count > 0) {
                std::uniform_int_distribution<size_t> dist_int(0, node->out_edges_count - 1);
                Edge* edge = node->out_edges[dist_int(rng)];
                if (edge) curr = edge->dest_id;
            } else {
                curr = start_node;
            }
        }
        path.push_back(curr);
    }

    return path;
}

// ======================================================================
// Influence Maximization (Independent Cascade Model)
// ======================================================================
static double estimate_spread(GraphEngine& ge, const std::unordered_set<node_id_t>& seed, size_t mc_simulations) {
    double total_influence = 0.0;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double p = 0.1; // active probability

    for (size_t sim = 0; sim < mc_simulations; ++sim) {
        std::unordered_set<node_id_t> active = seed;
        std::queue<node_id_t> q;
        for (node_id_t s : seed) q.push(s);

        while (!q.empty()) {
            node_id_t u = q.front();
            q.pop();

            Node* u_node = ge.get_node(u);
            if (!u_node) continue;

            for (size_t i = 0; i < u_node->out_edges_count; ++i) {
                Edge* edge = u_node->out_edges[i];
                if (edge) {
                    node_id_t v = edge->dest_id;
                    if (active.count(v) == 0) {
                        if (dist(rng) < p) {
                            active.insert(v);
                            q.push(v);
                        }
                    }
                }
            }
        }
        total_influence += active.size();
    }

    return total_influence / mc_simulations;
}

std::vector<node_id_t> GraphAnalytics::influence_maximization_greedy(
    GraphEngine& ge, size_t seed_size, size_t mc_simulations) {

    std::vector<node_id_t> result;
    std::unordered_set<node_id_t> seed;
    const auto& nodes = ge.get_all_nodes();

    for (size_t k = 0; k < seed_size && k < nodes.size(); ++k) {
        node_id_t best_node = 0;
        double max_spread = -1.0;

        for (const auto& [nid, n] : nodes) {
            if (seed.count(nid) == 0) {
                seed.insert(nid);
                double spread = estimate_spread(ge, seed, mc_simulations);
                seed.erase(nid);

                if (spread > max_spread) {
                    max_spread = spread;
                    best_node = nid;
                }
            }
        }

        if (best_node != 0) {
            seed.insert(best_node);
            result.push_back(best_node);
        }
    }

    return result;
}

// ======================================================================
// Link Prediction
// ======================================================================
std::vector<LinkPredictionScore> GraphAnalytics::predict_links(
    GraphEngine& ge, const std::string& method) {

    std::vector<LinkPredictionScore> predictions;
    const auto& nodes = ge.get_all_nodes();

    std::unordered_map<node_id_t, std::unordered_set<node_id_t>> neighbors;
    for (const auto& [nid, n] : nodes) {
        for (size_t i = 0; i < n->out_edges_count; ++i) {
            Edge* edge = n->out_edges[i];
            if (edge) {
                neighbors[nid].insert(edge->dest_id);
                neighbors[edge->dest_id].insert(nid);
            }
        }
    }

    std::vector<node_id_t> node_ids;
    for (const auto& [nid, n] : nodes) node_ids.push_back(nid);

    for (size_t i = 0; i < node_ids.size(); ++i) {
        for (size_t j = i + 1; j < node_ids.size(); ++j) {
            node_id_t u = node_ids[i];
            node_id_t v = node_ids[j];

            if (neighbors[u].count(v) > 0) continue; // edge already exists

            std::vector<node_id_t> common;
            for (node_id_t val : neighbors[u]) {
                if (neighbors[v].count(val) > 0) {
                    common.push_back(val);
                }
            }

            double score = 0.0;
            if (method == "jaccard") {
                std::unordered_set<node_id_t> union_set = neighbors[u];
                union_set.insert(neighbors[v].begin(), neighbors[v].end());
                if (!union_set.empty()) {
                    score = static_cast<double>(common.size()) / union_set.size();
                }
            } else if (method == "adamic_adar") {
                for (node_id_t c : common) {
                    double deg = neighbors[c].size();
                    if (deg > 1.0) {
                        score += 1.0 / std::log(deg);
                    }
                }
            } else if (method == "preferential_attachment") {
                score = static_cast<double>(neighbors[u].size() * neighbors[v].size());
            }

            if (score > 0.0) {
                predictions.push_back({u, v, score});
            }
        }
    }

    std::sort(predictions.begin(), predictions.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    return predictions;
}

} // namespace AetherGraph
