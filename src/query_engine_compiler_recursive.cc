#include "query_engine_compiler_recursive.h"
#include <queue>

namespace AetherGraph {

void QueryEngineCompilerRecursive::compile_recursive_expansion(
    GraphEngine& ge,
    node_id_t start_node,
    const RecursivePathFrame& frame,
    std::unordered_set<node_id_t>& visited_nodes) {

    visited_nodes.clear();
    std::queue<std::pair<node_id_t, size_t>> q;
    q.push({start_node, 0});
    visited_nodes.insert(start_node);

    while (!q.empty()) {
        auto [curr, depth] = q.front();
        q.pop();

        if (depth >= frame.max_depth) continue;

        // Traverse neighbors
        auto neighbors = ge.get_neighbors(curr);
        for (node_id_t nbr : neighbors) {
            if (visited_nodes.find(nbr) == visited_nodes.end()) {
                visited_nodes.insert(nbr);
                q.push({nbr, depth + 1});
            }
        }
    }
}

} // namespace AetherGraph
