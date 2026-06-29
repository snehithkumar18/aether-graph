#ifndef AETHER_GRAPH_QUERY_ENGINE_COMPILER_RECURSIVE_H
#define AETHER_GRAPH_QUERY_ENGINE_COMPILER_RECURSIVE_H

#include "execution_plans.h"
#include <unordered_set>
#include <vector>

namespace AetherGraph {

struct RecursivePathFrame {
    std::string edge_type;
    size_t min_depth;
    size_t max_depth;
};

class QueryEngineCompilerRecursive {
public:
    QueryEngineCompilerRecursive() = default;
    ~QueryEngineCompilerRecursive() = default;

    void compile_recursive_expansion(
        GraphEngine& ge,
        node_id_t start_node,
        const RecursivePathFrame& frame,
        std::unordered_set<node_id_t>& visited_nodes);
};

} // namespace AetherGraph

#endif // AETHER_GRAPH_QUERY_ENGINE_COMPILER_RECURSIVE_H
